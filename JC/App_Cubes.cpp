#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Random.h"
#include "JC/Render.h"
#include "JC/Unicode.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"
#include <stdio.h>
#include "stb/stb_image.h"
#include "JC/MinimalWindows.h"
#include "JC/Render_Vk.h"

#undef LoadImage

using namespace JC;

//--------------------------------------------------------------------------------------------------

constexpr ErrCode Err_LoadShader = { .ns = "app", .code = 1 };

//--------------------------------------------------------------------------------------------------

struct Vertex {
	Vec3  xyz   = {};
	float pad;
	Vec2  uv    = {};
	float pad2[2];
	Vec4  rgba  = {};
};

struct Mesh {
	Render::Buffer vertexBuffer     = {};
	U64            vertexBufferAddr = 0;
	U32            vertexCount      = 0;
	Render::Buffer indexBuffer      = {};
	U32            indexCount       = 0;
};

struct PushConstants {
	Mat4 model               = {};
	U64  vertexBufferAddr    = 0;
	U64  sceneBufferAddr     = 0;
	Vec4 color               = {};
	U32  imageIdx            = 0;
};

struct Scene {
	Mat4 view;
	Mat4 proj;
	Vec4 ambient;
};

struct Entity {
	Mesh mesh       = {};
	Vec3 pos        = {};
	Vec3 axis       = {};
	f32  angle      = 0.0f;
	f32  angleSpeed = 0.0f;
};

//--------------------------------------------------------------------------------------------------

Res<Render::Image> CreateDepthImage(U32 width, U32 height) {
	Render::Image image = {};
	if (Res<> r = Render::CreateImage(
		width,
		height,
		Render::ImageFormat::D32_Float,
		Render::ImageUsage::DepthAttachment,
		Render::Sampler{}
	).To(image); !r) {
		return r;
	}

	if (Res<> r = Render::BeginCmds(); !r) {
		Render::DestroyImage(image);
		return r;
	}

	Render::CmdImageBarrier(
		image,
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,	// TODO: test without read and see what happens
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
	);

	if (Res<> r = Render::EndCmds(); !r) { return r; }
	if (Res<> r = Render::SubmitCmds(); !r) { return r; }

	return image;
}

//--------------------------------------------------------------------------------------------------

Res<Mesh> CreateMesh() {
	Render::Buffer vertexBuffer = {};
	if (Res<> r = Render::CreateBuffer(24 * sizeof(Vertex), Render::BufferUsage::Storage).To(vertexBuffer); !r) {
		return r;
	}
	Render::Buffer indexBuffer = {};
	if (Res<> r = Render::CreateBuffer(36 * sizeof(U32), Render::BufferUsage::Index).To(indexBuffer); !r) {
		Render::DestroyBuffer(vertexBuffer);
		return r;
	}

	if (Res<> r = Render::BeginCmds(); !r) {
		return r;
	}

	Vertex* const vertices = (Vertex*)Render::CmdBeginBufferUpdate(vertexBuffer);
	// +Z
	vertices[ 0] = { .xyz = { -1.0f,  1.0f,  1.0f }, .uv = { 0.0, 0.0f }, .rgba = { 0.5f, 0.0f, 0.0f, 1.0f } };
	vertices[ 1] = { .xyz = {  1.0f,  1.0f,  1.0f }, .uv = { 1.0, 0.0f }, .rgba = { 0.5f, 0.0f, 0.0f, 1.0f } };
	vertices[ 2] = { .xyz = {  1.0f, -1.0f,  1.0f }, .uv = { 1.0, 1.0f }, .rgba = { 0.5f, 0.0f, 0.0f, 1.0f } };
	vertices[ 3] = { .xyz = { -1.0f, -1.0f,  1.0f }, .uv = { 0.0, 1.0f }, .rgba = { 0.5f, 0.0f, 0.0f, 1.0f } };
	// -Z
	vertices[ 4] = { .xyz = {  1.0f,  1.0f, -1.0f }, .uv = { 0.0, 0.0f }, .rgba = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 5] = { .xyz = { -1.0f,  1.0f, -1.0f }, .uv = { 1.0, 0.0f }, .rgba = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 6] = { .xyz = { -1.0f, -1.0f, -1.0f }, .uv = { 1.0, 1.0f }, .rgba = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 7] = { .xyz = {  1.0f, -1.0f, -1.0f }, .uv = { 0.0, 1.0f }, .rgba = { 0.0f, 0.5f, 0.0f, 1.0f } };
	// +X
	vertices[ 8] = { .xyz = {  1.0f,  1.0f,  1.0f }, .uv = { 0.0, 0.0f }, .rgba = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[ 9] = { .xyz = {  1.0f,  1.0f, -1.0f }, .uv = { 1.0, 0.0f }, .rgba = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[10] = { .xyz = {  1.0f, -1.0f, -1.0f }, .uv = { 1.0, 1.0f }, .rgba = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[11] = { .xyz = {  1.0f, -1.0f,  1.0f }, .uv = { 0.0, 1.0f }, .rgba = { 0.0f, 0.0f, 0.5f, 1.0f } };
	// -X
	vertices[12] = { .xyz = { -1.0f,  1.0f, -1.0f }, .uv = { 0.0, 0.0f }, .rgba = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[13] = { .xyz = { -1.0f,  1.0f,  1.0f }, .uv = { 1.0, 0.0f }, .rgba = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[14] = { .xyz = { -1.0f, -1.0f,  1.0f }, .uv = { 1.0, 1.0f }, .rgba = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[15] = { .xyz = { -1.0f, -1.0f, -1.0f }, .uv = { 0.0, 1.0f }, .rgba = { 0.5f, 0.5f, 0.0f, 1.0f } };
	// +Y
	vertices[16] = { .xyz = { -1.0f,  1.0f, -1.0f }, .uv = { 0.0, 0.0f }, .rgba = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[17] = { .xyz = {  1.0f,  1.0f, -1.0f }, .uv = { 1.0, 0.0f }, .rgba = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[18] = { .xyz = {  1.0f,  1.0f,  1.0f }, .uv = { 1.0, 1.0f }, .rgba = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[19] = { .xyz = { -1.0f,  1.0f,  1.0f }, .uv = { 0.0, 1.0f }, .rgba = { 0.5f, 0.0f, 0.5f, 1.0f } };
	// -Y
	vertices[20] = { .xyz = { -1.0f, -1.0f,  1.0f }, .uv = { 0.0, 0.0f }, .rgba = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[21] = { .xyz = {  1.0f, -1.0f,  1.0f }, .uv = { 1.0, 0.0f }, .rgba = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[22] = { .xyz = {  1.0f, -1.0f, -1.0f }, .uv = { 1.0, 1.0f }, .rgba = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[23] = { .xyz = { -1.0f, -1.0f, -1.0f }, .uv = { 0.0, 1.0f }, .rgba = { 0.0f, 0.5f, 0.5f, 1.0f } };

	Render::CmdEndBufferUpdate(vertexBuffer);


	U32* const indices = (U32*)Render::CmdBeginBufferUpdate(indexBuffer);
	// +Z
	indices[ 0] =  0;
	indices[ 1] =  1;
	indices[ 2] =  2;
	indices[ 3] =  0;
	indices[ 4] =  2;
	indices[ 5] =  3;
	// -Z
	indices[ 6] =  4;
	indices[ 7] =  5;
	indices[ 8] =  6;
	indices[ 9] =  4;
	indices[10] =  6;
	indices[11] =  7;
	// +X
	indices[12] =  8;
	indices[13] =  9;
	indices[14] = 10;
	indices[15] =  8;
	indices[16] = 10;
	indices[17] = 11;
	// -X
	indices[18] = 12;
	indices[19] = 13;
	indices[20] = 14;
	indices[21] = 12;
	indices[22] = 14;
	indices[23] = 15;
	// +Y
	indices[24] = 16;
	indices[25] = 17;
	indices[26] = 18;
	indices[27] = 16;
	indices[28] = 18;
	indices[29] = 19;
	// -Y
	indices[30] = 20;
	indices[31] = 21;
	indices[32] = 22;
	indices[33] = 20;
	indices[34] = 22;
	indices[35] = 23;
	Render::CmdEndBufferUpdate(indexBuffer);

	Render::CmdBufferBarrier(
		vertexBuffer,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_READ_BIT
	);

	Render::CmdBufferBarrier(
		indexBuffer,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_READ_BIT
	);

	if (Res<> r = Render::EndCmds(); !r) { return r; }
	if (Res<> r = Render::SubmitCmds(); !r) { return r; }

	return Mesh {
		.vertexBuffer     = vertexBuffer,
		.vertexBufferAddr = Render::GetBufferAddr(vertexBuffer),
		.vertexCount      = 24,
		.indexBuffer      = indexBuffer,
		.indexCount       = 36,
	};
}

//--------------------------------------------------------------------------------------------------

Res<Render::Image> LoadImage(Arena* arena, s8 path) {
	Span<u8> data;
	if (Res<> r = FS::ReadAll(arena, path).To(data); !r) { return r; }

	int width = 0;
	int height = 0;
	int channels = 0;
	u8* imageData = (u8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, &channels, 0);
	if (!imageData) {
		JC_PANIC("bad");
	}
	Assert(channels == 3 || channels == 4);
	Defer { stbi_image_free(imageData); };

	Render::Image image;
	if (Res<> r = Render::CreateImage(width, height, Render::ImageFormat::R8G8B8A8_UNorm, Render::ImageUsage::Sampled, Render::Sampler{}).To(image); !r) { return r; }

	if (Res<> r = Render::BeginCmds(); !r) { return r; }

	Render::CmdImageBarrier(
		image,
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	void* ptr = Render::CmdBeginImageUpdate(image);
	if (channels == 4) {
		MemCpy(ptr, imageData, width * height * channels);
	} else {
		u8* in = imageData;
		u8* out = (u8*)ptr;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 0xff;
			}
		}
	}
	Render::CmdEndImageUpdate(image);

	Render::CmdImageBarrier(
		image,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	if (Res<> r = Render::EndCmds(); !r) { return r; }
	if (Res<> r = Render::SubmitCmds(); !r) { return r; }

	return image;
}

//--------------------------------------------------------------------------------------------------

Res<Render::Shader> LoadShader(Arena* arena, s8 path) {
	Span<u8> data;
	if (Res<> r = FS::ReadAll(arena, path).To(data); !r) {
		return r.err->Push(Err_LoadShader, "path", path);
	}

	Render::Shader shader;
	if (Res<> r = Render::CreateShader(data.data, data.len).To(shader); !r) {
		return r.err->Push(Err_LoadShader, "path", path);
	}

	return shader;
}

//--------------------------------------------------------------------------------------------------

struct CubeApp : App {
	static constexpr U32 MaxEntities = 10000;

	Arena*           temp            = 0;
	Arena*           perm            = 0;
	Log*             log             = 0;
	U32              windowWidth     = 0;
	U32              windowHeight    = 0;
	Render::Image    depthImage      = {};
	Mesh             mesh            = {};
	Render::Image    image           = {};
	U32              imageIdx        = 0;
	Render::Shader   vertexShader    = {};
	Render::Shader   fragmentShader  = {};
	Render::Pipeline pipeline        = {};
	Render::Buffer   sceneBuffer     = {};
	U64              sceneBufferAddr = 0;
	Entity*          entities        = 0;
	Vec3             camPos          = { .x = 0.0f, .y = 0.0f, .z = -20.0f };
	Vec3             camX            = {};
	Vec3             camY            = {};
	Vec3             camZ            = {};
	Bool             recreateDepth   = false;

	Res<> Init(Arena* tempIn, Arena* permIn, Log* logIn, const Window::State* windowState) override {
		temp = tempIn;
		perm = permIn;
		log = logIn;
		windowWidth  = windowState->width;
		windowHeight = windowState->height;
		
		if (Res<> r = CreateDepthImage(windowState->width, windowState->height).To(depthImage); !r) { return r; }
		if (Res<> r = CreateMesh().To(mesh); !r) { return r; }
		if (Res<> r = LoadImage(temp, "Assets/texture.jpg").To(image); !r) { return r; }
		imageIdx = Render::BindImage(image);
		if (Res<> r = LoadShader(temp, "Shaders/mesh.vert.spv").To(vertexShader); !r) { return r; }
		if (Res<> r = LoadShader(temp, "Shaders/mesh.frag.spv").To(fragmentShader); !r) { return r; }

		if (Res<> r = Render::CreateGraphicsPipeline(
			{ vertexShader, fragmentShader },
			{ Render::GetSwapchainImageFormat() },
			Render::ImageFormat::D32_Float
		).To(pipeline); !r) { return r; }

		if (Res<> r = Render::CreateBuffer(sizeof(Scene), Render::BufferUsage::Storage).To(sceneBuffer); !r) { return r; }
		sceneBufferAddr = Render::GetBufferAddr(sceneBuffer);

		auto RandomNormal = []() {
			return Vec3::Normalize(Vec3 {
				.x = 0.1f * Random::NextF32(),
				.y = 0.1f + Random::NextF32(),
				.z = 0.1f + Random::NextF32(),
			});
		};

		auto RandomVec3 = [](f32 max) {
			return Vec3 {
				.x = (1.0f - 2.0f * Random::NextF32()) * max,
				.y = (1.0f - 2.0f * Random::NextF32()) * max,
				.z = (1.0f - 2.0f * Random::NextF32()) * max,
			};
		};

		entities = perm->AllocT<Entity>(MaxEntities);
		for (U32 i = 0; i < MaxEntities; i++) {
			entities[i].mesh       = mesh;
			entities[i].pos        = RandomVec3(100.0f);
			entities[i].axis       = RandomNormal();
			entities[i].angle      = 0.0f;
			entities[i].angleSpeed = Random::NextF32() / 15.0f;
		}

		return Ok();
	}

	void Shutdown() override {
		Render::WaitIdle();
		Render::DestroyImage(depthImage);
		Render::DestroyBuffer(mesh.vertexBuffer);
		Render::DestroyBuffer(mesh.indexBuffer);
		Render::DestroyImage(image);
		Render::DestroyShader(vertexShader);
		Render::DestroyShader(fragmentShader);
		Render::DestroyPipeline(pipeline);
		Render::DestroyBuffer(sceneBuffer);
	}

	struct Input {
		float xPos  = 0.0f;
		float xNeg  = 0.0f;
		float yPos  = 0.0f;
		float yNeg  = 0.0f;
		float zPos  = 0.0f;
		float zNeg  = 0.0f;
		float yaw   = 0.0f;
		float pitch = 0.0f;
	};
	Input input = {};

	Res<> Events(Span<Event::Event> events) override {
		input.yaw = 0.0f;
		input.pitch = 0.0f;
		for (const Event::Event* e = events.Begin(); e != events.End(); e++) {
			switch (e->type) {
				case Event::Type::Exit:
					Exit();
					break;
				case Event::Type::Key:
					     if (e->key.key == Event::Key::D)      { input.xPos = e->key.down ? 1.0f : 0.0f; }
					else if (e->key.key == Event::Key::A)      { input.xNeg = e->key.down ? 1.0f : 0.0f; }
					else if (e->key.key == Event::Key::Space)  { input.yPos = e->key.down ? 1.0f : 0.0f; }
					else if (e->key.key == Event::Key::C)      { input.yNeg = e->key.down ? 1.0f : 0.0f; }
					else if (e->key.key == Event::Key::S)      { input.zPos = e->key.down ? 1.0f : 0.0f; }
					else if (e->key.key == Event::Key::W)      { input.zNeg = e->key.down ? 1.0f : 0.0f; }
					else if (e->key.key == Event::Key::Escape) { Exit(); }
					break;
				case Event::Type::MouseMove:
					input.yaw   += -(float)e->mouseMove.x;
					input.pitch +=  (float)e->mouseMove.y;
					break;
				case Event::Type::WindowResized:
					windowWidth  = e->windowResized.width;
					windowHeight = e->windowResized.height;
					if (windowWidth && windowHeight) {
						recreateDepth = true;
					}
					break;
			}
		}

		return Ok();
	}

	static constexpr float camRotPerSec  = 0.03f;
	static constexpr float camMovePerSec = 20.0f;

	float camYaw = 0.0f;
	float camPitch = 0.0f;
	Res<> Update(double secs) override {
		const float fsecs = (float)secs;
		const float camMove = fsecs * camMovePerSec;
		const float camRot  = fsecs * camRotPerSec;

		camYaw   += input.yaw   * camRot;
		camPitch += input.pitch * camRot;

		camZ = Mat3::Mul(Mat3::RotateY(camYaw), Vec3 { 0.0f, 0.0f, -1.0f });
		camX = Vec3::Cross(Vec3 { 0.0f, 1.0f, 0.0f }, camZ);

		camPos = Vec3::AddScaled(camPos, camX, input.xPos * camMove - input.xNeg * camMove);
		camPos = Vec3::AddScaled(camPos, camY, input.yPos * camMove - input.yNeg * camMove);
		camPos = Vec3::AddScaled(camPos, camZ, input.zPos * camMove - input.zNeg * camMove);

		camZ = Mat3::Mul(Mat3::AxisAngle(camX, camPitch), camZ);
		camY = Vec3::Cross(camZ, camX);

		return Ok();
	}

	Res<> Draw() override {
		if (Res<> r = Render::BeginCmds(); !r) { return r; }

		if (recreateDepth) {
			Render::DestroyImage(depthImage);
			if (Res<> r = Render::CreateImage(
				windowWidth,
				windowHeight,
				Render::ImageFormat::D32_Float,
				Render::ImageUsage::DepthAttachment,
				Render::Sampler{}
			).To(depthImage); !r) {
				return r;
			}

			Render::CmdImageBarrier(
				depthImage,
				VK_PIPELINE_STAGE_2_NONE,
				VK_ACCESS_2_NONE,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,	// TODO: test without read and see what happens
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
			);
			recreateDepth = false;
		}

		const Render::Image swapchainImage = Render::GetSwapchainImage();
		Render::CmdImageBarrier(
			swapchainImage,
			VK_PIPELINE_STAGE_2_NONE,
			VK_ACCESS_2_NONE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		);

		Scene* const scene = (Scene*)Render::CmdBeginBufferUpdate(sceneBuffer);
		scene->view    = Mat4::Look(camPos, camX, camY, camZ);
		scene->proj    = Mat4::Perspective(DegToRad(45.0f), (f32)windowWidth / (f32)windowHeight, 0.01f, 1000.0f);
		scene->ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
		Mat4 proj = Mat4::Perspective(DegToRad(45.0f), (f32)windowWidth / (f32)windowHeight, 0.01f, 100000000.0f);

		Render::CmdEndBufferUpdate(sceneBuffer);

		const Render::Pass pass = {
			.pipeline         = pipeline,
			.colorAttachments = { swapchainImage },
			.depthAttachment  = depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .w = (f32)windowWidth, .h = (f32)windowHeight },
			.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		};

		Render::CmdBeginPass(&pass);

		Render::CmdBindIndexBuffer(mesh.indexBuffer);

		for (U64 i = 0; i < MaxEntities; i++) {
			PushConstants pushConstants = {
				.model               = Mat4::Mul(Mat4::RotateX(entities[i].angle), Mat4::Mul(Mat4::RotateY(entities[i].angle), Mat4::Translate(entities[i].pos))),
				.vertexBufferAddr    = mesh.vertexBufferAddr,
				.sceneBufferAddr     = sceneBufferAddr,
				.color               = {},
				.imageIdx            = 0,
			};
			entities[i].angle += entities[i].angleSpeed;
			Render::CmdPushConstants(pipeline, &pushConstants, sizeof(pushConstants));
			Render::CmdDrawIndexed(mesh.indexCount);
		}

		Render::CmdEndPass();

		Render::CmdImageBarrier(
			swapchainImage,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_NONE,
			VK_ACCESS_2_NONE,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		);

		if (Res<> r = Render::EndCmds(); !r) { return r; }
		//if (Res<> r = Render::SubmitCmds(); !r) { return r; }

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

int main(int argc, const char** argv) {
	CubeApp cubeApp;
	RunApp(&cubeApp, argc, argv);
	return 0;
}