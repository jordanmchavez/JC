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

constexpr s8 FileNameOnly(s8 path) {
	for (const char* i = path.data + path.len - 1; i >= path.data; i--) {
		if (*i == '/' || *i == '\\') {
			return s8(i + 1, path.data + path.len);
		}
	}
	return path;
}


//--------------------------------------------------------------------------------------------------

constexpr ErrCode Err_LoadShader = { .ns = "app", .code = 1 };

//--------------------------------------------------------------------------------------------------

struct Vertex {
	Vec3  pos    = {};
	u32   pad1;
	Vec3  normal = {};
	u32   pad2;
	Vec2  uv     = {};
	u32   pad3[2];
	Vec4  color  = {};
};

struct Mesh {
	Render::Buffer vertexBuffer     = {};
	u64            vertexBufferAddr = 0;
	u32            vertexCount      = 0;
	Render::Buffer indexBuffer      = {};
	u32            indexCount       = 0;
};

struct BindlessImage {
	Render::Image image       = {};
	u32           bindlessIdx = 0;
};

struct PushConstants {
	Mat4 model               = {};
	u64  vertexBufferAddr    = 0;
	u64  sceneBufferAddr     = 0;
	Vec4 color               = {};
	u32  imageIdx            = 0;
	u32  pad[3];
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

Res<Render::Image> CreateDepthImage(u32 width, u32 height) {
	Render::Image depthImage = {};
	if (Res<> r = Render::CreateImage(width, height, Render::ImageFormat::D32_F, Render::ImageUsage::DepthStencilAttachment, Render::Sampler{}).To(depthImage); !r) {
		return r.err;
	}

	if (Res<> r = Render::BeginCmds(); !r) {
		Render::DestroyImage(depthImage);
		return r.err;
	}

	Render::CmdImageBarrier(
		depthImage, 
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
	);

	if (Res<> r = Render::EndCmds(); !r) {
		Render::DestroyImage(depthImage);
		return r.err;
	}

	//Render::WaitIdle();

	return depthImage;
}

//--------------------------------------------------------------------------------------------------

Res<Mesh> CreateCubeMesh() {
	Render::Buffer vertexBuffer = {};
	if (Res<> r = Render::CreateBuffer(24 * sizeof(Vertex), Render::BufferUsage::Storage).To(vertexBuffer); !r) { return r.err; }
	if (Res<> r = Render::BeginCmds(); !r) { return r.err; }

	Render::BufferUpdate update = Render::CmdBeginBufferUpdate(vertexBuffer);
	Vertex* const vertices = (Vertex*)update.ptr;
	// +Z
	vertices[ 0] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 0.0, 0.0f }, .color = { 0.5f, 0.0f, 0.0f, 1.0f } };
	vertices[ 1] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 1.0, 0.0f }, .color = { 0.5f, 0.0f, 0.0f, 1.0f } };
	vertices[ 2] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 1.0, 1.0f }, .color = { 0.5f, 0.0f, 0.0f, 1.0f } };
	vertices[ 3] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 0.0, 1.0f }, .color = { 0.5f, 0.0f, 0.0f, 1.0f } };
	// -Z
	vertices[ 4] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 0.0, 0.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 5] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 1.0, 0.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 6] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 1.0, 1.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 7] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 0.0, 1.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	// +X
	vertices[ 8] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[ 9] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[10] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[11] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	// -X
	vertices[12] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[13] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[14] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[15] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	// +Y
	vertices[16] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[17] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[18] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[19] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	// -Y
	vertices[20] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[21] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[22] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[23] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
	Render::CmdEndBufferUpdate(update);

	Render::Buffer indexBuffer = {};
	if (Res<> r = Render::CreateBuffer(36 * sizeof(u32), Render::BufferUsage::Index).To(indexBuffer); !r) {
		Render::DestroyBuffer(vertexBuffer);
		return r.err;
	}

	update = Render::CmdBeginBufferUpdate(indexBuffer);
	u32* const indices = (u32*)update.ptr;
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
	Render::CmdEndBufferUpdate(update);

	if (Res<> r = Render::EndCmds(); !r) { return r.err; }
	if (Res<> r = Render::ImmediateSubmitCmds(); !r ) { return r.err; }

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
	if (Res<> r = FS::ReadAll(arena, path).To(data); !r) { return r.err; }

	int width = 0;
	int height = 0;
	int channels = 0;
	u8* imageData = (u8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, &channels, 0);
	if (!imageData) {
		Panic("bad");
	}
	Assert(channels == 3 || channels == 4);
	Defer { stbi_image_free(imageData); };

	Render::Image image;
	if (Res<> r = Render::CreateImage(width, height, Render::ImageFormat::R8G8B8A8_N, Render::ImageUsage::Sampled, Render::Sampler{}).To(image); !r) { return r.err; }

	if (Res<> r = Render::BeginCmds(); !r) { return r.err; }
	Render::CmdImageBarrier(
		image,
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT
	);

	Render::ImageUpdate update = Render::CmdBeginImageUpdate(image, width * 4);
	if (channels == 4) {
		MemCpy(update.ptr, imageData, width * height * channels);
	} else {
		u8* in = imageData;
		u8* out = (u8*)update.ptr;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 0xff;
			}
		}
	}
	Render::CmdEndImageUpdate(update);

	Render::CmdImageBarrier(
		image,
		VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
	);
	if (Res<> r = Render::EndCmds(); !r) { return r.err; }
	if (Res<> r = Render::ImmediateSubmitCmds(); !r ) { return r.err; }

	Render::BindImage(image, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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
	static constexpr u32 MaxEntities = 10000;

	Arena*           temp            = 0;
	Arena*           perm            = 0;
	Log*             log             = 0;
	u32              windowWidth     = 0;
	u32              windowHeight    = 0;
	Render::Image    depthImage      = {};
	Mesh             cubeMesh        = {};
	Render::Image    texture         = {};
	Render::Shader   vertexShader    = {};
	Render::Shader   fragmentShader  = {};
	Render::Pipeline pipeline        = {};
	Render::Buffer   sceneBuffer     = {};
	u64              sceneBufferAddr = 0;
	Entity*          entities        = 0;
	Vec3             camPos          = { .x = 0.0f, .y = 0.0f, .z = 20.0f };
	Vec3             camX            = {};
	Vec3             camY            = {};
	Vec3             camZ            = {};

	Res<> Init(Arena* tempIn, Arena* permIn, Log* logIn, const Window::State* windowState) override {
		temp = tempIn;
		perm = permIn;
		log = logIn;
		windowWidth  = windowState->width;
		windowHeight = windowState->height;

		if (Res<> r = CreateDepthImage(windowState->width, windowState->height).To(depthImage); !r) { return r; }

		if (Res<> r = CreateCubeMesh().To(cubeMesh); !r) { return r; }

		if (Res<> r = LoadImage(temp, "Assets/texture.jpg").To(texture); !r) { return r; }

		if (Res<> r = LoadShader(temp, "Shaders/mesh.vert.spv").To(vertexShader); !r) { return r; }

		if (Res<> r = LoadShader(temp, "Shaders/mesh.frag.spv").To(fragmentShader); !r) { return r; }

		if (Res<> r = Render::CreateGraphicsPipeline(
			{ vertexShader, fragmentShader },
			{ Render::GetSwapchainImageFormat() },
			Render::ImageFormat::D32_F
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
		for (u32 i = 0; i < MaxEntities; i++) {
			entities[i].mesh       = cubeMesh;
			entities[i].pos        = RandomVec3(100.0f);
			entities[i].axis       = RandomNormal();
			entities[i].angle      = 0.0f;
			entities[i].angleSpeed = Random::NextF32() / 15.0f;
		}

		return Ok();
	}

	void Shutdown() override {
		Render::DestroyImage(depthImage);
		Render::DestroyBuffer(cubeMesh.vertexBuffer);
		Render::DestroyBuffer(cubeMesh.indexBuffer);
		Render::DestroyImage(texture);
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
					Render::DestroyImage(depthImage);
					windowWidth  = e->windowResized.width;
					windowHeight = e->windowResized.height;
					if (windowWidth && windowHeight) {
						if (Res<> r = CreateDepthImage(windowWidth, windowHeight).To(depthImage); !r) {
							return r;
						}
					}
					break;
			}
		}

		return Ok();
	}

	static constexpr float camRotPerSec  = 0.08f;
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

		Render::CmdImageBarrier(
			Render::GetCurrentSwapchainImage(),
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			VK_ACCESS_2_NONE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
		);

		Render::BufferUpdate update = Render::CmdBeginBufferUpdate(sceneBuffer);
		Scene* const scene = (Scene*)update.ptr;
		scene->view    = Mat4::Look(camPos, camX, camY, camZ);
		scene->proj    = Mat4::Perspective(DegToRad(45.0f), (f32)windowWidth / (f32)windowHeight, 0.01f, 1000.0f);
		scene->ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
		Mat4 proj = Mat4::Perspective(DegToRad(45.0f), (f32)windowWidth / (f32)windowHeight, 0.01f, 100000000.0f);

		Render::CmdEndBufferUpdate(update);
		Render::CmdBufferBarrier(
			sceneBuffer,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, 
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		);

		Render::Pass pass = {
			.pipeline         = pipeline,
			.colorAttachments = { Render::GetCurrentSwapchainImage() },
			.depthAttachment  = depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .w = (f32)windowWidth, .h = (f32)windowHeight },
			.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		};

		Render::CmdBeginPass(&pass);

		Render::CmdBindIndexBuffer(cubeMesh.indexBuffer);

		for (u64 i = 0; i < MaxEntities; i++) {
			PushConstants pushConstants = {
				.model               = Mat4::Mul(Mat4::RotateX(entities[i].angle), Mat4::Mul(Mat4::RotateY(entities[i].angle), Mat4::Translate(entities[i].pos))),
				.vertexBufferAddr    = cubeMesh.vertexBufferAddr,
				.sceneBufferAddr     = sceneBufferAddr,
				.color               = {},
				.imageIdx            = 0,
			};
			entities[i].angle += entities[i].angleSpeed;
			Render::CmdPushConstants(pipeline, &pushConstants, sizeof(pushConstants));
			Render::CmdDrawIndexed(cubeMesh.indexCount);
		}

		Render::CmdEndPass();

		Render::CmdImageBarrier(
			Render::GetCurrentSwapchainImage(),
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			VK_ACCESS_2_NONE
		);

		if (Res<> r = Render::EndCmds(); !r) { return r; }

		return Ok();
	}

};

//--------------------------------------------------------------------------------------------------

int main(int argc, const char** argv) {
	CubeApp cubeApp;
	RunApp(&cubeApp, argc, argv);
	return 0;
}