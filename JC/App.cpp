#include "JC/App.h"

#include "JC/Config.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/Log.h"
#include "JC/Render.h"
#include "JC/Sys.h"
#include "JC/Time.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr ErrCode Err_Init = { .ns = "app", .code = 1 };

static Arena* temp;
static Arena* perm;
static Log*   log;

Res<> RunAppInternal(App* app, int argc, const char** argv) {
	Arena tempInst = CreateArena(Config::GetU64("App.TempArenaSize"));
	Arena permInst = CreateArena(Config::GetU64("App.PermArenaSize"));
	temp = &tempInst;
	perm = &permInst;

	log = GetLog();
	log->Init(temp);
	log->AddFn([](const char* msg, u64 len) {
		Sys::Print(s8(msg, len));
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

	Time::Init();

	if (argc == 2 && argv[1] == s8("test")) {
		UnitTest::Run(log);
		return Ok();
	}

	FS::Init(temp);

	Event::Init(log, temp);

	const Window::Style windowStyle = (Window::Style)Config::GetU64("App.WindowStyle");

	u32 displayIdx  = Config::GetU32("App.DisplayIdx");
	Span<Window::Display> displays = Window::GetDisplays();
	if (displayIdx >= displays.len) {
		displayIdx = 0;
	}
	const Window::Display* const display = &displays[displayIdx];

	i32 windowX      = 0;
	i32 windowY      = 0;
	u32 windowWidth  = 0;
	u32 windowHeight = 0;
	if (windowStyle != Window::Style::Fullscreen) {
		windowWidth  = Config::GetU32("App.WindowWidth");;
		windowHeight = Config::GetU32("App.WindowHeight");
	} else {
		if (windowWidth  > display->width)  { windowWidth  = display->width; }
		if (windowHeight > display->height) { windowHeight = display->height; }
		windowX = (i32)(display->width  - windowWidth)  / 2;
		windowY = (i32)(display->height - windowHeight) / 2;
	}

	Window::InitInfo windowInitInfo = {
		.temp                 = temp,
		.log                  = log,
		.title                = "test window",
		.style                = windowStyle,
		.x                    = windowX,
		.y                    = windowY,
		.width                = windowWidth,
		.height               = windowHeight,
		.fullscreenDisplayIdx = displayIdx,
	};
	if (Res<> r = Window::Init(&windowInitInfo); !r) {
		return r.Push(Err_Init);
	}

	Window::PumpMessages();
	Window::State windowState = Window::GetState();

	const Window::PlatformInfo windowPlatformInfo = Window::GetPlatformInfo();
	Render::InitInfo renderInitInfo = {
		.perm               = perm,
		.temp               = temp,
		.log                = log,
		.width              = windowState.width,
		.height             = windowState.height,
		.windowPlatformInfo = &windowPlatformInfo,
	};
	if (Res<> r = Render::Init(&renderInitInfo); !r) {
		return r;
	}

	const u64 startTicks = Time::Now();
	for(;;) {
		temp->Reset(0);

		const u64 ticks = Time::Now();
		const double secs = Time::Secs(ticks - startTicks);

		Window::PumpMessages();
		if (Window::IsExitRequested()) {
			Event::Add({ .type = Event::Type::Exit });
		}

		const Window::State prevWindowState = windowState;
		windowState = Window::GetState();
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

		Span<Event::Event> events = Event::Get();
		if (Res<> r = app->Events(events); !r) { return r; }
		Event::Clear();

		if (Res<> r = app->Update(secs); !r) { return r; }

		if (!windowState.minimized && w
		if (Res<> r = Render::BeginFrame(); !r) {
			if (r.err->ec == Render::Err_Resize) {
				if (r = Render::RecreateSwapchain(windowWidth, windowHeight); !r) {
					return r;
				}
			} else {
				return r;
			}
		}

		if (Res<> r = app->Draw(); !r) { return r; }
		
		if (Res<> r = Render::EndFrame(); !r) {
			if (r.err->ec == Render::Err_Resize) {
				if (r = Render::RecreateSwapchain(windowWidth, windowHeight); !r) {
					return r;
				}
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

void RunApp(App* app, int argc, const char** argv) {
	Res<> r = RunAppInternal(app, argc, argv);
	if (!r) {
		if (log) {
			Errorf(r.err);
		}
	}

	Shutdown();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC
