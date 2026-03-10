#include "JC/App.h"

#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Effect.h"
#include "JC/File.h"
#include "JC/Gpu.h"
#include "JC/Input.h"
#include "JC/Log.h"
#include "JC/Rng.h"
#include "JC/StrDb.h"
#include "JC/Sys.h"
#include "JC/Time.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"

namespace JC::App {

//--------------------------------------------------------------------------------------------------

static void PanicFn(SrcLoc sl, char const* expr, char const* msg) {
	char buf[1024];
	char* iter = buf;
	char* end = buf + sizeof(buf) - 1;
	iter = SPrintf(iter, end, "\n***PANIC***\n");
	iter = SPrintf(iter, end, "%s(%u)\n", sl.file, sl.line);
	if (expr) {
		iter = SPrintf(iter, end, "expr: '%s'\n", expr);
	}
	if (msg) {
		iter = SPrintf(iter, end, "%s\n", msg);
	}
	iter--;
	Logf("%s", Str(buf, (U32)(iter - buf)));

	if (Sys::DbgPresent()) {
		DbgBreak;
	}

	Sys::Abort();
}

//--------------------------------------------------------------------------------------------------

static Mem permMem;
static Mem tempMem;
Res<> RunImpl(App* app, int argc, char const* const* argv) {
	SetPanicFn(PanicFn);

	permMem = Mem::Create(16 * GB);
	tempMem = Mem::Create(16 * GB);

	constexpr U64 MaxActionIds = 1024;
	U64* actionIds = Mem::AllocT<U64>(permMem, MaxActionIds);

	Err::SetBreakOnErr(true);
	Time::Init();
	File::Init(tempMem);
	StrDb::Init();
	U64 rngSeed = Time::Now();
	rngSeed = 0x14bd05373a90;
	Rng::Seed(rngSeed);

	Cfg::Init(permMem, argc, argv);

	Log::Init(tempMem);
	Log::AddFn([](Log::Msg const* msg) {
		StrBuf sb(tempMem);
		sb.Printf("%s%s(%u): %s", msg->level == Log::Level::Error ? "!!! " : "", msg->sl.file, msg->sl.line, Str(msg->line, msg->lineLen));
		Sys::Print(sb.ToStr());
		if (Sys::DbgPresent()) {
			Sys::DbgPrint(sb.ToStrZ());
		}
	});

	Logf("Rng seed = 0x%016x", rngSeed);

	Input::Init(permMem);

	Try(app->PreInit(permMem, tempMem));

	Window::InitDef const windowInitDef = {
		.permMem    = permMem,
		.tempMem    = tempMem,
		.title      = Cfg::GetStr(Cfg_Title, "Untitled"),
		.style      = (Window::Style)Cfg::GetU32(Cfg_WindowStyle, (U32)Window::Style::BorderedResizable),
		.width      = Cfg::GetU32(Cfg_WindowWidth, 1024),
		.height     = Cfg::GetU32(Cfg_WindowHeight, 768),
		.displayIdx = Cfg::GetU32(Cfg_WindowDisplayIdx, 1),
	};
	Try(Window::Init(&windowInitDef));

	Window::Update();
	Window::State windowState = Window::GetState();

	Window::PlatformDef const windowPlatformDef = Window::GetPlatformDef();
	const Gpu::InitDef gpuInitDef = {
		.permMem           = permMem,
		.tempMem           = tempMem,
		.windowWidth       = windowState.width,
		.windowHeight      = windowState.height,
		.windowPlatformDef = &windowPlatformDef,
	};
	Try(Gpu::Init(&gpuInitDef));

	Draw::InitDef const drawInitDef = {
		.permMem      = permMem,
		.tempMem      = tempMem,
		.windowWidth  = windowState.width,
		.windowHeight = windowState.height,
	};
	Try(Draw::Init(&drawInitDef));

	Try(app->Init(&windowState));

	U64 frame = 0;
	U64 lastTicks = Time::Now();
	for (;;) {
		frame++;

		Mem::Reset(tempMem, MemMark());
		Err::Update(frame);

		U64 const nowTicks = Time::Now();
		F32 const sec = (F32)Time::Secs(nowTicks - lastTicks);
		lastTicks = nowTicks;

		Window::Events const windowEvents = Window::Update();
		Window::State const prevWindowState = windowState;	// TODO: this could be rolled into Events as part of the return val from Window::Update()
		windowState = Window::GetState();

		Span<U64 const> const actions = Input::ProcessKeyEvents(windowEvents.keyEvents, actionIds, MaxActionIds);

		UpdateData const appUpdateData = {
			.sec         = sec,
			.actions     = actions,
			.mouseX      = windowEvents.mouseX,
			.mouseY      = windowEvents.mouseY,
			.mouseDeltaX = windowEvents.mouseDeltaX,
			.mouseDeltaY = windowEvents.mouseDeltaY,
			.exit        = windowEvents.exitEvent,
		};
		if (Res<> r = app->Update(&appUpdateData); !r) {
			if (r.err == Err_Exit) {
				return Ok();
			}
			return r.err;
		}

		bool recreatedSwapChain = false;
		if (prevWindowState.width != windowState.width || prevWindowState.height != windowState.height) {
			Logf("Window size changed: %ux%u -> %ux%u", prevWindowState.width, prevWindowState.height, windowState.width, windowState.height);
			Try(Gpu::RecreateSwapchain(windowState.width, windowState.height));
			recreatedSwapChain = true;
			Try(Draw::ResizeWindow(windowState.width, windowState.height));
			Try(app->ResizeWindow(windowState.width, windowState.height));
			continue;
		}

		if (windowState.minimized || windowState.width == 0 || windowState.height == 0) {
			continue;
		}

		Gpu::FrameData gpuFrameData;
		if (Res<> r = Gpu::BeginFrame().To(gpuFrameData); !r) {
			if (r.err != Gpu::Err_RecreateSwapchain) {
				return r;
			}
			Try(Gpu::RecreateSwapchain(windowState.width, windowState.height));
			Logf("Recreated swapchain after BeginFrame() with w=%u, h=%u", windowState.width, windowState.height);
			continue;
		}

		Draw::BeginFrame(&gpuFrameData);	// TODO: move to App.cpp

		Try(app->Draw());

		Draw::EndFrame();
		
		if (Res<> r = Gpu::EndFrame(); !r) {
			if (r.err != Gpu::Err_RecreateSwapchain) {
				return r;
			}
			Try(Gpu::RecreateSwapchain(windowState.width, windowState.height));
			Logf("Recreated swapchain after EndFrame() with w=%u, h=%u", windowState.width, windowState.height);
		}
	}
}

//--------------------------------------------------------------------------------------------------

void Shutdown(App* app) {
	Gpu::WaitIdle();
	app->Shutdown();
	Draw::Shutdown();
	Gpu::Shutdown();
	Window::Shutdown();
}

//--------------------------------------------------------------------------------------------------

bool Run(App* app, int argc, char const* const* argv) {
	if (argc == 2 && argv[1] == Str("test")) {
		UnitTest::Run(); return 0;
	}

	Res<> r = RunImpl(app, argc, argv);
	if (!r) {
		LogErr(r);
	}
	Shutdown(app);
	return (bool)r;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::App