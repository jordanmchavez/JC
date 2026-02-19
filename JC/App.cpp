#include "JC/App.h"

#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Event.h"
#include "JC/FS.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/StrDb.h"
#include "JC/Sys.h"
#include "JC/Time.h"
#include "JC/Unit.h"
#include "JC/Window.h"

namespace JC::App {

//--------------------------------------------------------------------------------------------------

static bool exitRequested = false;

void RequestExit() {
	exitRequested = true;
}

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

Res<> RunImpl(App* app, int argc, char const* const* argv) {
	SetPanicFn(PanicFn);

	Mem permMem = Mem::Create(16 * GB);
	Mem tempMem = Mem::Create(16 * GB);

	Time::Init();
	FS::Init(tempMem);
	StrDb::Init();

	Cfg::Init(permMem, argc, argv);

	Log::Init(tempMem);
	Log::AddFn([](Log::Msg const* msg) {
		Sys::Print(Str(msg->line, msg->lineLen));
		if (Sys::DbgPresent()) {
			Sys::DbgPrint(msg->line);
		}
	});

	Try(app->PreInit(permMem, tempMem));

	Window::InitDesc const windowInitDesc = {
		.tempMem    = tempMem,
		.title      = Cfg::GetStr(Cfg_Title, "Untitled"),
		.style      = (Window::Style)Cfg::GetU32(Cfg_WindowStyle, (U32)Window::Style::BorderedResizable),
		.width      = Cfg::GetU32(Cfg_WindowWidth, 1024),
		.height     = Cfg::GetU32(Cfg_WindowHeight, 768),
		.displayIdx = Cfg::GetU32(Cfg_WindowDisplayIdx, 1),
	};
	Try(Window::Init(&windowInitDesc));

	Window::Frame();
	Window::State windowState = Window::GetState();

	Window::PlatformDesc const windowPlatformDesc = Window::GetPlatformDesc();
	const Gpu::InitDesc gpuInitDesc = {
		.permMem            = permMem,
		.tempMem            = tempMem,
		.windowWidth        = windowState.width,
		.windowHeight       = windowState.height,
		.windowPlatformDesc = &windowPlatformDesc,
	};
	Try(Gpu::Init(&gpuInitDesc));

	Draw::InitDesc const drawInitDesc = {
		.permMem      = permMem,
		.tempMem      = tempMem,
		.windowWidth  = windowState.width,
		.windowHeight = windowState.height,
	};
	Try(Draw::Init(&drawInitDesc));

	Try(app->Init(&windowState));

	U64 frame = 0;
	U64 lastTicks = Time::Now();
	exitRequested = false;
	while (!exitRequested) {
		frame++;

		Mem::Reset(tempMem, MemMark());
		Err::Frame(frame);

		U64 const nowTicks = Time::Now();
		U64 const deltaTicks = nowTicks - lastTicks;
		lastTicks = nowTicks;

		Window::Frame();
		Window::State const prevWindowState = windowState;
		windowState = Window::GetState();

		Try(app->Update(deltaTicks));

		if (prevWindowState.width != windowState.width || prevWindowState.height != windowState.height) {
			Try(Gpu::RecreateSwapchain(windowState.width, windowState.height));
			Logf("Window size changed: %ux%u -> %ux%u", prevWindowState.width, prevWindowState.height, windowState.width, windowState.height);
		}

		bool recreatedSwapChain = false;
		Gpu::Frame gpuFrame;
		if (Res<> r = Gpu::BeginFrame().To(gpuFrame); !r) {
			if (r.err != Gpu::Err_RecreateSwapchain) {
				return r;
			}
			Try(Gpu::RecreateSwapchain(windowState.width, windowState.height));
			Logf("Recreated swapchain after BeginFrame() with w=%u, h=%u", windowState.width, windowState.height);
			recreatedSwapChain = true;
		}

		if (!recreatedSwapChain && !windowState.minimized && windowState.width > 0 && windowState.height > 0) {
			Try(app->Draw(&gpuFrame));
		
			if (Res<> r = Gpu::EndFrame(); !r) {
				if (r.err != Gpu::Err_RecreateSwapchain) {
					return r;
				}
				Logf("Recreated swapchain after EndFrame() with w=%u, h=%u", windowState.width, windowState.height);
			}
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	Gpu::WaitIdle();
	Draw::Shutdown();
	Gpu::Shutdown();
	Window::Shutdown();
}

//--------------------------------------------------------------------------------------------------

bool Run(App* app, int argc, char const* const* argv) {
	Res<> r = RunImpl(app, argc, argv);
	if (!r) {
		LogErr(r);
	}
	Shutdown();
	return (bool)r;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::App