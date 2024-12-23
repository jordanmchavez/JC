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

static Arena  tempInst;
static Arena  permInst;
static Arena* temp;
static Arena* perm;
static Log*   log;
static bool   exit;

//--------------------------------------------------------------------------------------------------

void App::Exit() { exit = true; }

static void AppPanicFn(SrcLoc sl, s8 expr, s8 fmt, VArgs args) {
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
}
//--------------------------------------------------------------------------------------------------

static Res<> RunAppInternal(App* app, int argc, const char** argv) {
	tempInst = CreateArena((u64) 4 * 1024 * 1024 * 1024);
	permInst = CreateArena((u64)16 * 1024 * 1024 * 1024);
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

	SetPanicFn(AppPanicFn);

	Time::Init();
	FS::Init(temp);
	Config::Init(perm);

	if (argc == 2 && argv[1] == s8("test")) {
		UnitTest::Run(log);
		return Ok();
	}

	Event::Init(log);

	const Window::Style windowStyle = (Window::Style)Config::GetU32("App.WindowStyle", (u32)Window::Style::BorderedResizable);
	Window::InitInfo windowInitInfo = {
		.temp       = temp,
		.log        = log,
		.title      = Config::GetS8("App.WindowTitlye", "test window"),
		.style      = windowStyle,
		.width      = windowStyle == Window::Style::Fullscreen ? 0 : Config::GetU32("App.WindowWidth",  1600),
		.height     = windowStyle == Window::Style::Fullscreen ? 0 : Config::GetU32("App.WindowHeight", 1200),
		.displayIdx = Config::GetU32("App.DisplayIdx", 0),
	};
	if (Res<> r = Window::Init(&windowInitInfo); !r) {
		return r.Push(Err_Init);
	}

	Window::PumpMessages();
	Window::State windowState = Window::GetState();

	const Window::PlatformDesc windowPlatformDesc = Window::GetPlatformDesc();
	Render::InitDesc renderInitDesc = {
		.perm               = perm,
		.temp               = temp,
		.log                = log,
		.width              = windowState.width,
		.height             = windowState.height,
		.windowPlatformDesc = &windowPlatformDesc,
	};
	if (Res<> r = Render::Init(&renderInitDesc); !r) {
		return r;
	}

	if (Res<> r = app->Init(perm, temp, log, &windowState); !r) {
		return r;
	}

	u64 lastTicks = Time::Now();
	for (exit = false; !exit ;) {
		temp->Reset(0);

		const u64 ticks = Time::Now();
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
			if (Res<> r = Render::RecreateSwapchain(windowState.width, windowState.height); !r) {
				return r;
			}
			Logf("Window size changed: {}x{} -> {}x{}", prevWindowState.width, prevWindowState.height, windowState.width, windowState.height);
		}

		if (Res<> r = Render::BeginFrame(); !r) {
			if (r.err->ec != Render::Err_RecreateSwapchain) {
				return r;
			}
			if (r = Render::RecreateSwapchain(windowState.width, windowState.height); !r) {
				return r;
			}
			Logf("Recreated swapchain after BeginFrame() with w={}, h={}", windowState.width, windowState.height);
			continue;
		}

		if (Res<> r = app->Draw(); !r) { return r; }
		
		if (Res<> r = Render::EndFrame(); !r) {
			if (r.err->ec != Render::Err_RecreateSwapchain) {
				return r;
			}
			if (r = Render::RecreateSwapchain(windowState.width, windowState.height); !r) {
				return r;
			}
			Logf("Recreated swapchain after EndFrame() with w={}, h={}", windowState.width, windowState.height);
			continue;
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown(App* app) {
	app->Shutdown();
	Render::Shutdown();
	Window::Shutdown();
}

//--------------------------------------------------------------------------------------------------

void RunApp(App* app, int argc, const char** argv) {
	Res<> r = RunAppInternal(app, argc, argv);
	if (!r) {
		if (log) {
			Errorf(r.err);
		}
	}

	Shutdown(app);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC