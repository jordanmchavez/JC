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
#include "JC/MinimalWindows.h"
#include "JC/Render_Vk.h"

using namespace JC;

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

static Arena* temp;
static Log*   log;

//--------------------------------------------------------------------------------------------------

constexpr s8 FileNameOnly(s8 path) {
	for (const char* i = path.data + path.len - 1; i >= path.data; i--) {
		if (*i == '/' || *i == '\\') {
			return s8(i + 1, path.data + path.len);
		}
	}
	return path;
}

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
	vertices[ 2] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 0.0, 1.0f }, .color = { 0.5f, 0.0f, 0.0f, 1.0f } };
	vertices[ 3] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 1.0, 1.0f }, .color = { 0.5f, 0.0f, 0.0f, 1.0f } };
	// -Z
	vertices[ 4] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 0.0, 0.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 5] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 1.0, 0.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 6] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 0.0, 1.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	vertices[ 7] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 1.0, 1.0f }, .color = { 0.0f, 0.5f, 0.0f, 1.0f } };
	// +X
	vertices[ 8] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[ 9] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[10] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	vertices[11] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.0f, 0.0f, 0.5f, 1.0f } };
	// -X
	vertices[12] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[13] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[14] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	vertices[15] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.5f, 0.5f, 0.0f, 1.0f } };
	// +Y
	vertices[16] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[17] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[18] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	vertices[19] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.5f, 0.0f, 0.5f, 1.0f } };
	// -Y
	vertices[20] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 0.0, 0.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[21] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 1.0, 0.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[22] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 0.0, 1.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
	vertices[23] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 1.0, 1.0f }, .color = { 0.0f, 0.5f, 0.5f, 1.0f } };
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

Res<Render::Shader> LoadShader(s8 path) {
	Span<u8> data;
	if (Res<> r = FS::ReadAll(temp, path).To(data); !r) {
		return r.err->Push(Err_LoadShader, "path", path);
	}

	Render::Shader shader;
	if (Res<> r = Render::CreateShader(data.data, data.len).To(shader); !r) {
		return r.err->Push(Err_LoadShader, "path", path);
	}

	return shader;
}

//--------------------------------------------------------------------------------------------------

Res<> Run(int argc, const char** argv) {
	Arena tempInst = CreateArena((u64)4 * 1024 * 1024 * 1024);
	temp = &tempInst;

	Arena permInst = CreateArena((u64)16 * 1024 * 1024 * 1024);
	Arena* perm = &permInst;

	log = GetLog();
	log->Init(temp);
	log->AddFn([](const char* msg, u64 len) {
		fwrite(msg, 1, len, stdout);
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(msg);
		}
	});

	SetPanicFn([](SrcLoc sl, s8 expr, s8 fmt, VArgs args) {
		char msg[1024];
		char* iter = msg;
		char* end = msg + sizeof(msg) - 1;
		iter = Fmt(iter, end, "\n***PANIC***\n");
		iter = Fmt(iter, end, "{}({})\n", sl.file, sl.line);
		if (expr.len > 0) {
			iter = Fmt(iter, end, "expr: '{}'\n", expr);
		}
		if (fmt.len > 0) {
			iter = VFmt(iter, end, fmt, args);
			iter = Fmt(iter, end, "\n");
		}
		iter--;
		Logf("{}", s8(msg, (u64)(iter - msg)));

		if (Sys::IsDebuggerPresent()) {
			Sys_DebuggerBreak();
		}

		Sys::Abort();
	});

	if (argc == 2 && argv[1] == s8("test")) {
		UnitTest::Run(log);
		return Ok();
	}

	FS::Init(temp);
	Event::Init(log, temp);

	u32 windowWidth = 1600;
	u32 windowHeight = 1200;
	Window::InitInfo windowInitInfo = {
		.temp              = temp,
		.log               = log,
		.title             = "test window",
		.style             = Window::Style::BorderedResizable,
		.rect              = Rect { .x = 100, .y = 100, .width = (i32)windowWidth, .height = (i32)windowHeight },
		.fullscreenDisplay = 0,
	};
	if (Res<> r = Window::Init(&windowInitInfo); !r) {
		return r;
	}

	Window::PlatformInfo windowPlatformInfo = Window::GetPlatformInfo();
	Render::InitInfo renderInitInfo = {
		.perm               = perm,
		.temp               = temp,
		.log                = log,
		.width              = windowWidth,
		.height             = windowHeight,
		.windowPlatformInfo = &windowPlatformInfo,
	};
	if (Res<> r = Render::Init(&renderInitInfo); !r) {
		return r;
	}

	Render::Image depthImage = {};
	if (Res<> r = CreateDepthImage(windowWidth, windowHeight).To(depthImage); !r) { return r; }

	Mesh cubeMesh = {};
	if (Res<> r = CreateCubeMesh().To(cubeMesh); !r) { return r; }

	Render::Shader vertexShader = {};
	if (Res<> r = LoadShader("Shaders/mesh.vert.spv").To(vertexShader); !r) { return r; }

	Render::Shader fragmentShader = {};
	if (Res<> r = LoadShader("Shaders/mesh.frag.spv").To(fragmentShader); !r) { return r; }

	Render::Pipeline pipeline = {};
	if (Res<> r = Render::CreateGraphicsPipeline(
		{ vertexShader, fragmentShader },
		{ Render::GetSwapchainImageFormat() },
		Render::ImageFormat::D32_F
	).To(pipeline); !r) { return r; }

	Render::Buffer sceneBuffer = {};
	if (Res<> r = Render::CreateBuffer(sizeof(Scene), Render::BufferUsage::Storage).To(sceneBuffer); !r) { return r; }
	const u64 sceneBufferAddr = Render::GetBufferAddr(sceneBuffer);

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

	constexpr u32 MaxEntities = 20000;
	Entity* entities = perm->AllocT<Entity>(MaxEntities);
	for (u32 i = 0; i < MaxEntities; i++) {
		entities[i].mesh       = cubeMesh;
		entities[i].pos        = RandomVec3(200.0f);
		entities[i].axis       = RandomNormal();
		entities[i].angle      = 0.0f;
		entities[i].angleSpeed = Random::NextF32() / 5.0f;
	}

	bool exitRequested = false;
	Vec3 camPos = { .x = 0.0f, .y = 0.0f, .z = 20.0f };
	Vec3 camVelocity = {};
	f32 camRotX = 0.0f;
	f32 camRotY = 0.0f;
	constexpr f32 camSpeed = 10.0f;
	Mat4 proj = Mat4::Perspective(DegToRad(45.0f), (f32)windowWidth / (f32)windowHeight, 0.01f, 100000000.0f);
	while (!exitRequested) {
		temp->Reset(0);

		Window::PumpMessages();
		if (Window::IsExitRequested()) {
			Event::Add({ .type = Event::Type::Exit });
		}

		Span<Event::Event> events = Event::Get();
		for (const Event::Event* e = events.Begin(); e != events.End(); e++) {
			switch (e->type) {
				case Event::Type::Exit:
					exitRequested = true;
					break;
				case Event::Type::Key:
					       if (e->key.key == Event::Key::A)     { camVelocity.x = e->key.down ? -camSpeed : 0.0f;
					} else if (e->key.key == Event::Key::D)     { camVelocity.x = e->key.down ?  camSpeed : 0.0f;
					} else if (e->key.key == Event::Key::Space) { camVelocity.y = e->key.down ?  camSpeed : 0.0f;
					} else if (e->key.key == Event::Key::C)     { camVelocity.y = e->key.down ? -camSpeed : 0.0f;
					} else if (e->key.key == Event::Key::W)     { camVelocity.z = e->key.down ? -camSpeed : 0.0f;
					} else if (e->key.key == Event::Key::S)     { camVelocity.z = e->key.down ?  camSpeed : 0.0f;

					} else if (e->key.key == Event::Key::Escape) { exitRequested = true; }
					break;
				case Event::Type::MouseMove:
					camRotX += (f32)e->mouseMove.y *  0.001f;
					camRotY += (f32)e->mouseMove.x * -0.001f;
					break;
			}
		}
		Event::Clear();

		Vec3 camX = { .x = 1.0f, .y = 0.0f, .z = 0.0f };
		Vec3 camY = { .x = 0.0f, .y = 1.0f, .z = 0.0f };
		Vec3 camZ = { .x = 0.0f, .y = 0.0f, .z = 1.0f };

		camZ = Mat3::Mul(Mat3::RotateY(camRotY), camZ);
		camX = Vec3::Cross(camY, camZ);

		camPos = Vec3::AddScaled(camPos, camX, camVelocity.x);
		camPos = Vec3::AddScaled(camPos, camY, camVelocity.y);
		camPos = Vec3::AddScaled(camPos, camZ, camVelocity.z);

		camZ = Mat3::Mul(Mat3::AxisAngle(camX, camRotX), camZ);
		camY = Vec3::Cross(camZ, camX);

		if (Res<> r = Render::BeginFrame(); !r) { return r; }
		if (Res<> r = Render::BeginCmds(); !r) { return r; }

		const Render::Image swapchainImage = Render::GetCurrentSwapchainImage();

		Render::CmdImageBarrier(
			swapchainImage,
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
			.colorAttachments = { swapchainImage },
			.depthAttachment  = depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .width = (f32)windowWidth, .height = (f32)windowHeight },
			.scissor          = { .x = 0,    .y = 0,    .width = (i32)windowWidth, .height = (i32)windowHeight },
		};

		Render::CmdBeginPass(&pass);

		Render::CmdBindIndexBuffer(cubeMesh.indexBuffer);

		for (u64 i = 0; i < MaxEntities; i++) {
			PushConstants pushConstants = {
				//.model               = Mat4::Mul(Mat4::AxisAngle(entities[i].axis, entities[i].angle), Mat4::Translate(entities[i].pos)),
				//.model               = Mat4::Mul(Mat4::RotateY(entities[i].angle), Mat4::Translate(entities[i].pos)),
				.model               = Mat4::Mul(Mat4::RotateX(entities[i].angle), 
					Mat4::Mul(Mat4::RotateY(entities[i].angle), Mat4::Translate(entities[i].pos))
				),
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
			swapchainImage,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			VK_ACCESS_2_NONE
		);

		if (Res<> r = Render::EndCmds(); !r) { return r; }

		if (Res<> r = Render::EndFrame(); !r) {
			if (r.err->ec == Render::Err_Resize) {
				Window::State windowState = Window::GetState();
				windowWidth  = windowState.rect.width;
				windowHeight = windowState.rect.height;
				if (r = Render::ResizeSwapchain(windowWidth, windowHeight); !r) { return r; }
				Render::DestroyImage(depthImage);
				if (r = CreateDepthImage(windowWidth, windowHeight).To(depthImage); !r) { return r; }
			} else {
				return r;
			}
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
}

//--------------------------------------------------------------------------------------------------

int main(int argc, const char** argv) {
	if (Res<> r = Run(argc, argv); !r) {
		if (log) {
			Errorf(r.err);
		}
	}
	Shutdown();
	return 0;
}