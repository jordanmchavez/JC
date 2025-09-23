#include "JC/Config.h"
#include "JC/Fmt.h"
#include "JC/Game.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Sys.h"
#include "JC/Time.h"
#include "JC/Ui.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr Str Cfg_Title        = "App.Title";
static constexpr Str Cfg_WindowStyle  = "App.WindowStyle";
static constexpr Str Cfg_WindowWidth  = "App.WindowWidth";
static constexpr Str Cfg_WindowHeight = "App.WindowHeight";
static constexpr Str Cfg_DisplayIdx   = "App.DisplayIdx";

JC_EC(App, Init);

static Allocator*     allocator;
static TempAllocator* tempAllocator;
static Log::Logger*   logger;
static Bool           exitRequested;

//--------------------------------------------------------------------------------------------------

static void PanicFn(SrcLoc sl, const char* expr, const char* msg) {
	if (logger) {
		char buf[1024];
		char* iter = buf;
		char* end = buf + sizeof(buf) - 1;
		iter = Fmt::Printf(iter, end, "\n***PANIC***\n");
		iter = Fmt::Printf(iter, end, "{}({})\n", sl.file, sl.line);
		if (expr) {
			iter = Fmt::Printf(iter, end, "expr: '{}'\n", expr);
		}
		if (msg) {
			iter = Fmt::Printf(iter, end, "{}\n", msg);
		}
		iter--;
		JC_LOG("{}", Str(buf, (U64)(iter - buf)));
	}

	if (Sys::IsDebuggerPresent()) {
		JC_DEBUGGER_BREAK;
	}

	Sys::Abort();
}
//--------------------------------------------------------------------------------------------------

static Res<> RunAppInternal(int argc, const char** argv) {
	SetPanicFn(PanicFn);

	Core::Init();
	allocator = g_allocator;
	tempAllocator = g_tempAllocator;
	Sys::Init(tempAllocator);

	logger = Log::InitLogger(tempAllocator);
	Log::AddFn([](const char* msg, U64 len) {
		Sys::Print(Str(msg, len));
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(msg);
		}
	});

	Time::Init();
	Config::Init(allocator);

	if (argc == 2 && argv[1] == Str("test")) {
		UnitTest::Run(tempAllocator, logger);
		return Ok();
	}

	if (Res<> r = Game::PreInit(allocator, tempAllocator, logger); !r) {
		return JC_PUSH_ERR(r.err, Init);
	}

	const Window::Style windowStyle = (Window::Style)Config::GetU32(App::Cfg_WindowStyle, (U32)Window::Style::BorderedResizable);
	Window::InitDesc windowInitDesc = {
		.tempAllocator = tempAllocator,
		.logger        = logger,
		.title         = Config::GetStr(App::Cfg_Title, "Untitled"),
		.style         = windowStyle,
		.width         = windowStyle == Window::Style::Fullscreen ? 0 : Config::GetU32(App::Cfg_WindowWidth,  1600),
		.height        = windowStyle == Window::Style::Fullscreen ? 0 : Config::GetU32(App::Cfg_WindowHeight, 1200),
		.displayIdx    = Config::GetU32(App::Cfg_DisplayIdx, 0),
	};
	if (Res<> r = Window::Init(&windowInitDesc); !r) {
		return r.err.Push(Err_Init());
	}

	Window::PumpMessages();
	Window::State windowState = Window::GetState();

	const Window::PlatformDesc windowPlatformDesc = Window::GetPlatformDesc();
	const Gpu::InitDesc gpuInitDesc = {
		.allocator          = allocator,
		.tempAllocator      = tempAllocator,
		.logger             = logger,
		.windowWidth        = windowState.width,
		.windowHeight       = windowState.height,
		.windowPlatformDesc = &windowPlatformDesc,
	};
	if (Res<> r = Gpu::Init(&gpuInitDesc); !r) {
		return r;
	}

	if (Res<> r = fns->Init(&windowState); !r) {
		return r;
	}

	U64 lastTicks = Time::Now();
	for (exitRequested = false; !exitRequested;) {
		tempAllocator->Reset();

		const U64 nowTicks = Time::Now();
		const U64 deltaTicks = nowTicks - lastTicks;
		lastTicks = nowTicks;

		Window::PumpMessages();

		const Window::State prevWindowState = windowState;
		windowState = Window::GetState();
		if (windowState.minimized || !windowState.width || !windowState.height) {
			continue;
		}


		Span<const Window::Event> events = Window::GetEvents();
		if (Res<> r = fns->Update(deltaTicks, events); !r) { return r; }
		Window::ClearEvents();

		if (windowState.minimized || !windowState.width || !windowState.height) {
			continue;
		}

		if (prevWindowState.width != windowState.width || prevWindowState.height != windowState.height) {
			if (Res<> r = Gpu::RecreateSwapchain(windowState.width, windowState.height); !r) {
				return r;
			}
			JC_LOG("Window size changed: {}x{} -> {}x{}", prevWindowState.width, prevWindowState.height, windowState.width, windowState.height);
		}

		bool skipFrame = false;
		Gpu::Frame frame;
		if (Res<> r = Gpu::BeginFrame().To(frame); !r) {
			if (r.err == Gpu::EC_RecreateSwapchain) {
				if (r = Gpu::RecreateSwapchain(windowState.width, windowState.height); !r) {
					return r;
				}
				JC_LOG("Recreated swapchain after BeginFrame() with w={}, h={}", windowState.width, windowState.height);
				skipFrame = true;
			} else {
				return r;
			}
		}
		if (skipFrame) { continue; }

		if (Res<> r = fns->Draw(frame); !r) { return r; }
		
		if (Res<> r = Gpu::EndFrame(); !r) {
			if (r.err == Gpu::EC_RecreateSwapchain) {
				if (r = Gpu::RecreateSwapchain(windowState.width, windowState.height); !r) {
					return r;
				}
				JC_LOG("Recreated swapchain after BeginFrame() with w={}, h={}", windowState.width, windowState.height);
			} else {
				return r;
			}
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	Gpu::WaitIdle();
	fns->Shutdown();
	Gpu::Shutdown();
	Window::Shutdown();
}

//--------------------------------------------------------------------------------------------------

void Main(int argc, const char** argv) {
	if (Res<> r = RunAppInternal(argc, argv); !r) {
		if (logger) {
			JC_LOG_ERROR("{}", r.err.GetStr());
		}
	}

	Shutdown();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC

//--------------------------------------------------------------------------------------------------

int main(int argc, const char* argv[])
{
	JC::Main(argc, argv);
	return 0;
}