#include "JC/App.h"

#include "JC/Config.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Render.h"
#include "JC/Sys.h"
#include "JC/Time.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

DefErr(App, Init);

static Mem::Allocator*     permAllocator;
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;
static Bool                exit;

//--------------------------------------------------------------------------------------------------

void App::Exit() { exit = true; }

static void AppPanicFn(const char* expr, const char* msg, Span<const NamedArg> namedArgs, SrcLoc sl) {
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
	for (U64 i = 0; i < namedArgs.len; i++) {
		iter = Fmt::Printf(iter, end, "  {}={}", namedArgs[i].name, namedArgs[i].arg);
	}
	iter--;
	Logf("{}", Str(buf, (U64)(iter - buf)));

	if (Sys::IsDebuggerPresent()) {
		Sys_DebuggerBreak();
	}

	Sys::Abort();
}
//--------------------------------------------------------------------------------------------------

static Res<> RunAppInternal(App* app, int argc, const char** argv) {
	Mem::Init(
		(U64)8 * 1024 * 1024,
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

	if (Res<> r = app->PreInit(permAllocator, tempAllocator, logger); !r) {
		return r.err.Push(Err_Init());
	}

	const Window::Style windowStyle = (Window::Style)Config::GetU32("App.WindowStyle", (U32)Window::Style::BorderedResizable);
	Window::InitDesc windowInitDesc = {
		.tempAllocator = tempAllocator,
		.logger        = logger,
		.title         = Config::GetStr("App.WindowTitle", "Untitled"),
		.style         = windowStyle,
		.width         = windowStyle == Window::Style::Fullscreen ? 0 : Config::GetU32("App.WindowWidth",  1600),
		.height        = windowStyle == Window::Style::Fullscreen ? 0 : Config::GetU32("App.WindowHeight", 1200),
		.displayIdx    = Config::GetU32("App.DisplayIdx", 0),
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

	const Render::InitDesc renderInitDesc = {
		.allocator     = permAllocator,
		.tempAllocator = tempAllocator,
		.logger        = logger,
		.windowWidth   = windowState.width,
		.windowHeight  = windowState.height,
	};
	if (Res<> r = Render::Init(&renderInitDesc); !r) { return r; }

	if (Res<> r = app->Init(&windowState); !r) {
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
		if (Res<> r = app->Events(events); !r) { return r; }
		Event::Clear();

		if (Res<> r = app->Update(secs); !r) { return r; }

		if (windowState.minimized || !windowState.width || !windowState.height) {
			continue;
		}

		if (prevWindowState.width != windowState.width || prevWindowState.height != windowState.height) {
			if (Res<> r = Render::WindowResized(windowState.width, windowState.height); !r) {
				return r;
			}
			Logf("Window size changed: {}x{} -> {}x{}", prevWindowState.width, prevWindowState.height, windowState.width, windowState.height);
		}

		if (Res<> r = Render::BeginFrame(); !r) {
			if (r.err == Render::EC_SkipFrame) {
				continue;
			}
			return r;
		}

		if (Res<> r = app->Draw(); !r) { return r; }
		
		if (Res<> r = Render::EndFrame(); !r) {
			if (r.err == Render::EC_SkipFrame) {
				continue;
			}
			return r;
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

namespace Gpu { void EndCommands(); }	// TODO: this sucks, fix

void Shutdown(App* app) {
	Gpu::EndCommands();
	Gpu::WaitIdle();
	app->Shutdown();
	Render::Shutdown();
	Gpu::Shutdown();
	Window::Shutdown();
}

//--------------------------------------------------------------------------------------------------

void RunApp(App* app, int argc, const char** argv) {
	if (Res<> r = RunAppInternal(app, argc, argv); !r) {
		if (logger) {
			Errorf("{}", r.err.GetStr());
		}
	}

	Shutdown(app);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC