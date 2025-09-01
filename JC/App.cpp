#include "JC/App.h"

#include "JC/Config.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Sys.h"
#include "JC/Time.h"
#include "JC/Ui.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"

namespace JC::App {

//--------------------------------------------------------------------------------------------------

DefErr(App, Init);

static Mem::Allocator*     permAllocator;
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;
static Bool                exit;

//--------------------------------------------------------------------------------------------------

void App::Exit() { exit = true; }

static void AppPanicFn(SrcLoc sl, const char* expr, const char* msg) {
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

	if (Sys::IsDebuggerPresent()) {
		JC_DEBUGGER_BREAK();
	}

	Sys::Abort();
}
//--------------------------------------------------------------------------------------------------

static Res<> RunAppInternal(Fns* fns, int argc, const char** argv) {
	Mem::Init(
		(U64)        8 * 1024 * 1024,
		(U64)16 * 1024 * 1024 * 1024,
		(U64)16 * 1024 * 1024 * 1024
	);
	permAllocator = Mem::permAllocator;
	tempAllocator = Mem::tempAllocator;

	Err::Init(tempAllocator);
	Sys::Init(tempAllocator);
	SetPanicFn(AppPanicFn);

	logger = Log::InitLogger(tempAllocator);
	Log::AddFn([](const char* msg, U64 len) {
		Sys::Print(Str(msg, len));
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(msg);
		}
	});

	Time::Init();
	Config::Init(permAllocator);

	if (argc == 2 && argv[1] == Str("test")) {
		UnitTest::Run(tempAllocator, logger);
		return Ok();
	}

	Event::Init(logger);

	if (Res<> r = fns->PreInit(permAllocator, tempAllocator, logger); !r) {
		return r.err.Push(Err_Init());
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
		.allocator          = permAllocator,
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
	for (exit = false; !exit ;) {
		tempAllocator->Reset();

		const U64 ticks = Time::Now();
		const double secs = Time::Secs(ticks - lastTicks);
		lastTicks = ticks;

		Window::PumpMessages();
		if (Window::IsExitRequested()) {
			Event::Add({ .type = Event::Type::Exit });
		}

		const Window::State prevWindowState = windowState;
		windowState = Window::GetState();
		if (windowState.minimized || !windowState.width || !windowState.height) {
			continue;
		}

		if (!prevWindowState.minimized && windowState.minimized) {
			Event::Add(Event::Event { .type = Event::Type::WindowMinimized });
		} else if (prevWindowState.minimized && !windowState.minimized) {
			Event::Add(Event::Event { .type = Event::Type::WindowRestored});
		}
		if (!prevWindowState.focused && windowState.focused) {
			Event::Add(Event::Event { .type = Event::Type::WindowFocused });
		} else if (prevWindowState.focused && !windowState.focused) {
			Event::Add(Event::Event { .type = Event::Type::WindowUnfocused});
		}

		if (prevWindowState.width != windowState.width || prevWindowState.height != windowState.height) {
			Event::Add(Event::Event {
				.type = Event::Type::WindowResized,
				.windowResized = {
					.width  = windowState.width,
					.height = windowState.height,
				},
			});
		}

		Span<Event::Event> events = Event::Get();
		if (Res<> r = Ui::Events(events); !r) { return r; }
		if (Res<> r = fns->Events(events); !r) { return r; }
		Event::Clear();

		if (Res<> r = fns->Update(secs); !r) { return r; }

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

void Shutdown(Fns* fns) {
	Gpu::WaitIdle();
	fns->Shutdown();
	Gpu::Shutdown();
	Window::Shutdown();
}

//--------------------------------------------------------------------------------------------------

void Run(Fns* fns, int argc, const char** argv) {
	if (Res<> r = RunAppInternal(fns, argc, argv); !r) {
		if (logger) {
			JC_LOG_ERROR("{}", r.err.GetStr());
		}
	}

	Shutdown(fns);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::App