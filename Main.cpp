#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/File.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Mem.h"
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

struct LogLeakReporter : MemLeakReporter {
	Log* log = 0;

	void LeakedScope(s8 name, u64 leakedBytes, u32 leakedAllocs, u32 leakedChildren) override {
		Errorf("Leaked scope: name={}, leakedBytes={}, leakedAllocs={}, leakedChildren={}", name, leakedBytes, leakedAllocs, leakedChildren);
	}

	void LeakedAlloc(SrcLoc sl, u64 leakedBytes, u32 leakedAllocs) override {
		Errorf("  {}({}): leakedBytes={}, leakedAllocs={}", sl.file, sl.line, leakedBytes, leakedAllocs);
	}

	void LeakedChild(s8 name, u64 leakedBytes, u32 leakedAllocs) override {
		Errorf("  Leaked child: name={}, leakedBytes={}, leakedAllocs={}", name, leakedBytes, leakedAllocs);
	}
};

//--------------------------------------------------------------------------------------------------

static DisplayApi*     displayApi;
static EventApi*       eventApi;
static FileApi*        fileApi;
static LogApi*         logApi;
static Log*            log;
static LogLeakReporter logLeakReporter;
static MemApi*         memApi;
static Render::Api*    renderApi;
static TempMem*        tempMem;
static WindowApi*      windowApi;

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

Res<Render::Shader> LoadShader(s8 path) {
	Span<u8> data;
	if (Res<> r = fileApi->ReadAll(tempMem, path).To(data); !r) {
		return r.err->Push(Err_LoadShader, "path", path);
	}

	Render::Shader shader;
	if (Res<> r = renderApi->CreateShader(data.data, data.len).To(shader); !r) {
		return r.err->Push(Err_LoadShader, "path", path);
	}

	return shader;
}

//--------------------------------------------------------------------------------------------------

Res<> Run(int argc, const char** argv) {
	memApi = GetMemApi();
	memApi->Init();
	tempMem = memApi->Temp();

	logApi = GetLogApi();
	logApi->Init(tempMem);
	logApi->AddFn([](const char* msg, u64 len) {
		fwrite(msg, 1, len, stdout);
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(msg);
		}
	});
	log = logApi->GetLog();

	logLeakReporter.log = log;
	memApi->SetLeakReporter(&logLeakReporter);

	SetPanicFn([](SrcLoc sl, s8 expr, s8 fmt, Args args) {
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
		UnitTest::Run(log, memApi);
		return Ok();
	}

	fileApi = GetFileApi();
	fileApi->Init(tempMem);

	eventApi = GetEventApi();
	eventApi->Init(log, tempMem);

	displayApi = GetDisplayApi();
	if (Res<> r = displayApi->Init(log); !r) {
		return r;
	}

	windowApi = GetWindowApi();
	WindowApiInit windowApiInit = {
		.displayApi        = displayApi,
		.eventApi          = eventApi,
		.log               = log,
		.tempMem           = tempMem,
		.title             = "test window",
		.windowMode        = WindowMode::BorderedResizable,
		.rect              = Rect { .x = 100, .y = 100, .width = 800, .height = 600 },
		.fullscreenDisplay = 0,
	};
	if (Res<> r = windowApi->Init(&windowApiInit); !r) {
		return r;
	}

	Mem* renderMem = memApi->Create("Mem", 0);
	renderApi = Render::GetApi();
	WindowPlatformData windowPlatformData = windowApi->GetPlatformData();
	Render::ApiInit renderApiInit = {
		.log                = log,
		.mem                = renderMem,
		.tempMem            = tempMem,
		.width              = 800,
		.height             = 600,
		.windowPlatformData = &windowPlatformData,
	};
	if (Res<> r = renderApi->Init(&renderApiInit); !r) {
		return r;
	}

	struct PushConstants {
		Mat4 model              = {};
		u64  vertexBufferPtr    = 0;
		u64  sceneBufferPtr     = 0;
		u64  materialsBufferPtr = 0;
		u32  materialIdx        = 0;
	};

	struct SceneData {
		Mat4 view;
		Mat4 proj;
		Vec4 ambient;
	};

	Render::Shader vertexShader = {};
	if (Res<> r = LoadShader("Shaders/mesh.vert.spv").To(vertexShader); !r) { return r; }

	Render::Shader fragmentShader = {};
	if (Res<> r = LoadShader("Shaders/mesh.frag.spv").To(fragmentShader); !r) { return r; }

	Render::Pipeline pipeline = {};
	if (Res<> r = renderApi->CreatePipeline(vertexShader, fragmentShader, sizeof(PushConstants)).To(pipeline); !r) { return r; }

	Render::Buffer sceneBuffer = {};
	if (Res<> r = renderApi->CreateBuffer(
		sizeof(SceneData),
		Render::BufferFlags::Index | Render::BufferFlags::Static | Render::BufferFlags::TransferDst
	).To(sceneBuffer); !r) { return r; }

	Render::Buffer sceneStagingBuffer = {};
	if (Res<> r = renderApi->CreateBuffer(
		sizeof(SceneData),
		Render::BufferFlags::Staging | Render::BufferFlags::TransferSrc
	).To(sceneStagingBuffer); !r) { return r; }

	u64 frame = 0;
	bool exitRequested = false;
	Vec3 eye = { .x = 0.0f, .y = 0.0f, .z = 5.0f };
	Vec3 look = { .x = 0.0f, .y = 0.0f, .z = -1.0f };
	Vec3 eyeVelocity = {};
	float eyeRotY = 0.0f;
	constexpr float camVelocity = 0.002f;
	Mat4 proj = Mat4::Perspective(DegToRad(45.0f), 800.0f / 600.0f, 0.01f, 1000.0f);
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

		if (Res<> r = renderApi->BeginFrame(); !r) { return r; }

		SceneData sceneData = {
			.view = Mat4::LookAt(eye, at, up),
			.proj = Mat4::Perspective(DegToRad(45.0f), (float)windowState.rect.width / (float)windowState.rect.height, 0.01f, 1000.0f),
			.ambient = { 0.1f, 0.1, 0.1, 1.0f },
		};
		MemCpy(sceneStagingBufferPtr, &sceneData, sizeof(sceneData));
		renderApi->CmdCopyBuffer(sceneStagingBuffer, sceneBuffer);
		renderApi->CmdBarrier(sceneBuffer, transfer/write -> vs+fs/read);
		renderApi->CmdBeginRendering(drawImage);

		renderApi->CmdBindPipeline(renderPipeline);
		for (u64 i = 0; i < entities.len; i++) {
			Entity* const entity = &entities[i];
			renderApi->CmdBindIndexBuffer(entity->mesh.indexBuffer);
			EntityPushConstants entityPushConstants = {
			};
			renderApi->CmdPushConstants(&entityPushConstants, sizeof(entityPushConstants));
			renderApi->CmdDraw(...);
		}
		renderApi->CmdEndRendering();

		if (Res<> r = renderApi->EndFrame(drawImage); !r) {
			if (r.err->ec == RenderApi::Err_Resize) {
				WindowState windowState = windowApi->GetState();
				if (r = renderApi->ResizeSwapchain(windowState.rect.width, windowState.rect.height); r) {
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