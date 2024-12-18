#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Render.h"
#include "JC/Unicode.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"
#include <stdio.h>
#include "JC/MinimalWindows.h"

using namespace JC;

//--------------------------------------------------------------------------------------------------

constexpr ErrCode Err_LoadShader = { .ns = "app", .code = 1 };

//--------------------------------------------------------------------------------------------------

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

struct Material {
	BindlessImage bindlessImage;
	Vec4 color;
};

struct PushConstants {
	Mat4 model               = {};
	u64  vertexBufferAddr    = 0;
	u64  sceneBufferAddr     = 0;
	u64  materialsBufferAddr = 0;
	u32  materialIdx         = 0;
};

struct SceneData {
	Mat4 view;
	Mat4 proj;
	Vec4 ambient;
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

struct Vertex {
	Vec3  pos    = {};
	Vec3  normal = {};
	Vec2  uv     = {};
};

/*
      Y+
      |
      |
      |
      |
      |
      |
      +------------------X+
     /
    /
   /
  /
 /
Z+
*/

Res<Mesh> CreateCubeMesh() {
	Render::Buffer vertexBuffer = {};
	if (Res<> r = Render::CreateBuffer(24 * sizeof(Vertex), Render::BufferUsage::Storage).To(vertexBuffer); !r) {
		return r.err;
	}

	Render::BufferUpdate update;
	if (Res<> r = Render::BeginBufferUpdate(vertexBuffer).To(update); !r) {
		Render::DestroyBuffer(vertexBuffer);
		return r.err;
	}
	Vertex* const vertices = (Vertex*)update.ptr;
	// +Z
	vertices[ 0] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 0.0, 0.0f } };
	vertices[ 1] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 1.0, 0.0f } };
	vertices[ 2] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 0.0, 1.0f } };
	vertices[ 3] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  0.0f,  1.0f }, .uv = { 1.0, 1.0f } };
	// -Z
	vertices[ 4] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 0.0, 0.0f } };
	vertices[ 5] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 1.0, 0.0f } };
	vertices[ 6] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 0.0, 1.0f } };
	vertices[ 7] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  0.0f,  0.0f, -1.0f }, .uv = { 1.0, 1.0f } };
	// +X
	vertices[ 8] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 0.0, 0.0f } };
	vertices[ 9] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 1.0, 0.0f } };
	vertices[10] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 0.0, 1.0f } };
	vertices[11] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  1.0f,  0.0f,  0.0f }, .uv = { 1.0, 1.0f } };
	// -X
	vertices[12] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 0.0, 0.0f } };
	vertices[13] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 1.0, 0.0f } };
	vertices[14] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 0.0, 1.0f } };
	vertices[15] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = { -1.0f,  0.0f,  0.0f }, .uv = { 1.0, 1.0f } };
	// +Y
	vertices[16] = { .pos = { -1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 0.0, 0.0f } };
	vertices[17] = { .pos = {  1.0f,  1.0f, -1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 1.0, 0.0f } };
	vertices[18] = { .pos = {  1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 0.0, 1.0f } };
	vertices[29] = { .pos = { -1.0f,  1.0f,  1.0f }, .normal = {  0.0f,  1.0f,  0.0f }, .uv = { 1.0, 1.0f } };
	// -Y
	vertices[20] = { .pos = { -1.0f, -1.0f,  1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 0.0, 0.0f } };
	vertices[21] = { .pos = {  1.0f, -1.0f,  1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 1.0, 0.0f } };
	vertices[22] = { .pos = {  1.0f, -1.0f, -1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 0.0, 1.0f } };
	vertices[23] = { .pos = { -1.0f, -1.0f, -1.0f }, .normal = {  0.0f, -1.0f,  0.0f }, .uv = { 1.0, 1.0f } };

	Render::EndBufferUpdate(update);

	Render::Buffer indexBuffer = {};
	if (Res<> r = Render::CreateBuffer(36 * sizeof(u32), Render::BufferUsage::Index).To(indexBuffer); !r) {
		Render::DestroyBuffer(vertexBuffer);
		return r.err;
	}

	if (Res<> r = Render::BeginBufferUpdate(indexBuffer).To(update); !r) {
		Render::DestroyBuffer(vertexBuffer);
		Render::DestroyBuffer(indexBuffer);
	}

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

	Render::EndBufferUpdate(update);

	return Mesh {
		.vertexBuffer     = vertexBuffer,
		.vertexBufferAddr = Render::GetBufferAddr(vertexBuffer),
		.vertexCount      = 24,
		.indexBuffer      = indexBuffer,
		.indexCount       = 36,
	};
}

//--------------------------------------------------------------------------------------------------

Res<Render::Shader> LoadShader(s8 path, s8 entry) {
	Span<u8> data;
	if (Res<> r = FS::ReadAll(temp, path).To(data); !r) {
		return r.err->Push(Err_LoadShader, "path", path);
	}

	Render::Shader shader;
	if (Res<> r = Render::CreateShader(data.data, data.len, "main").To(shader); !r) {
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

	u32 windowWidth = 800;
	u32 windowHeight = 600;
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

	Mesh cubeMesh = {};
	if (Res<> r = CreateCubeMesh().To(cubeMesh); !r) { return r; }

	Render::Shader vertexShader = {};
	if (Res<> r = LoadShader("Shaders/mesh.vert.spv").To(vertexShader); !r) { return r; }

	Render::Shader fragmentShader = {};
	if (Res<> r = LoadShader("Shaders/mesh.frag.spv").To(fragmentShader); !r) { return r; }

	Render::Pipeline pipeline = {};
	if (Res<> r = Render::CreatePipeline(vertexShader, fragmentShader, sizeof(PushConstants)).To(pipeline); !r) { return r; }

	Render::Buffer sceneBuffer = {};
	if (Res<> r = Render::CreateBuffer(sizeof(SceneData), Render::BufferUsage::Storage).To(sceneBuffer); !r) { return r; }

	const u64 sceneBufferAddr = Render::GetBufferAddr(sceneBuffer);
	Render::Buffer sceneStagingBuffer = {};
	if (Res<> r = Render::CreateBuffer(sizeof(SceneData), Render::BufferUsage::Staging).To(sceneStagingBuffer); !r) { return r; }

	void* sceneStagingBufferPtr = 0;
	if (Res<> r = Render::MapBuffer(sceneStagingBuffer).To(sceneStagingBufferPtr); !r) { return r; }

	u64 frame = 0;
	bool exitRequested = false;
	Vec3 eye = { .x = 0.0f, .y = 0.0f, .z = 5.0f };
	Vec3 look = { .x = 0.0f, .y = 0.0f, .z = -1.0f };
	Vec3 eyeVelocity = {};
	float eyeRotY = 0.0f;
	constexpr float camVelocity = 0.002f;
	Mat4 proj = Mat4::Perspective(DegToRad(45.0f), (float)windowWidth / (float)windowHeight, 0.01f, 1000.0f);
	while (!exitRequested) {
		memApi->Frame(frame);

		windowApi->PumpMessages();
		if (windowApi->IsExitRequested()) {
			eventApi->AddEvent({ .type = EventType::Exit });
		}

		Span<Event> events = eventApi->GetEvents();
		for (const Event* e = events.Begin(); e != events.End(); e++) {
			switch (e->type) {
				case EventType::Exit:
					exitRequested = true;
					break;
				case EventType::Key:
					if (e->key.key == EventKey::W) {
						eyeVelocity.z = e->key.down ? camVelocity : 0.0f;
					} else if (e->key.key == EventKey::S) {
						eyeVelocity.z = e->key.down ? -camVelocity : 0.0f;
					} else if (e->key.key == EventKey::A) {
						eyeVelocity.x = e->key.down ? camVelocity : 0.0f;
					} else if (e->key.key == EventKey::D) {
						eyeVelocity.x = e->key.down ? -camVelocity : 0.0f;
					} else if (e->key.key == EventKey::Escape) {
						exitRequested = true;
					}
					break;
				case EventType::MouseMove:
					eyeRotY += (float)e->mouseMove.x * -0.001f;
					break;
			}
		}
		eventApi->ClearEvents();

		constexpr Vec3 up = { .x = 0.0f, .y = 1.0f, .z = 0.0f };
		const Vec3 rotatedLook = Mat4::Mul(Mat4::RotateY(eyeRotY), look);
		const Vec3 rotatedLookOrtho = Vec3::Cross(up, rotatedLook);
		eye = Vec3::AddScaled(eye, rotatedLook, eyeVelocity.z);
		eye = Vec3::AddScaled(eye, rotatedLookOrtho, eyeVelocity.x);
		const Vec3 at = Vec3::Add(eye, rotatedLook);

		if (Res<> r = Render::BeginFrame(); !r) { return r; }

		SceneData sceneData = {
			.view = Mat4::LookAt(eye, at, up),
			.proj = Mat4::Perspective(DegToRad(45.0f), (float)windowWidth / (float)windowHeight, 0.01f, 1000.0f),
			.ambient = { 0.1f, 0.1, 0.1, 1.0f },
		};
		MemCpy(sceneStagingBufferPtr, &sceneData, sizeof(sceneData));
	
		Render::CmdCopyBuffer(sceneStagingBuffer, sceneBuffer);
		Render::CmdBarrier(sceneBuffer, transfer/write -> vs+fs/read);
		Render::CmdBeginRendering(drawImage);

		Render::CmdBindPipeline(renderPipeline);
		for (u64 i = 0; i < entities.len; i++) {
			Entity* const entity = &entities[i];
			Render::CmdBindIndexBuffer(entity->mesh.indexBuffer);
			EntityPushConstants entityPushConstants = {
			};
			Render::CmdPushConstants(&entityPushConstants, sizeof(entityPushConstants));
			Render::CmdDraw(...);
		}
		Render::CmdEndRendering();

		if (Res<> r = Render::EndFrame(drawImage); !r) {
			if (r.err->ec == RenderApi::Err_Resize) {
				WindowState windowState = windowApi->GetState();
				if (r = Render::ResizeSwapchain(windowState.rect.width, windowState.rect.height); r) {
					continue;
				}
			}
			return r;
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