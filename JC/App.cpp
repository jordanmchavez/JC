#include "JC/App.h"

#include "JC/Config.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/Log.h"
#include "JC/Render.h"
#include "JC/Sys.h"
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

	u32 windowX      = 0;
	u32 windowY      = 0;
	u32 windowWidth  = 0;
	u32 windowHeight = 0;
	if (windowStyle != Window::Style::Fullscreen) {
		windowWidth  = Config::GetU32("App.WindowWidth");;
		windowHeight = Config::GetU32("App.WindowHeight");
	} else {
		if (windowWidth  > display->width)  { windowWidth  = display->width; }
		if (windowHeight > display->height) { windowHeight = display->height; }
		windowX = (display->width  - windowWidth)  / 2;
		windowY = (display->height - windowHeight) / 2;
	}

	Window::InitInfo windowInitInfo = {
		.temp                 = temp,
		.log                  = log,
		.title                = "test window",
		.style                = Window::Style::BorderedResizable,
		.x                    = windowX,
		.y                    = windowY,
		.width                = windowWidth,
		.height               = windowHeight,
		.fullscreenDisplayIdx = displayIdx,
	};
	if (Res<> r = Window::Init(&windowInitInfo); !r) {
		return r.Push(Err_Init);
	}

	const Window::PlatformInfo windowPlatformInfo = Window::GetPlatformInfo();
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

	Time time = Sys::Now();
	for(;;) {
		temp->Reset(0);

		Window::PumpMessages();
		if (Window::IsExitRequested()) {
			Event::Add({ .type = Event::Type::Exit });
		}

		Span<Event::Event> events = Event::Get();
		app->Events(events);
		Event::Clear();

		if (Res<> r = Render::BeginFrame(); !r) { return r; }

		if (Res<> r = Render::EndFrame(); !r) {
			/*if (r.err->ec == Render::Err_Resize) {
				Window::State windowState = Window::GetState();
				windowWidth  = windowState.rect.width;
				windowHeight = windowState.rect.height;
				if (r = Render::ResizeSwapchain(windowWidth, windowHeight); !r) { return r; }
				Render::DestroyImage(depthImage);
				if (r = CreateDepthImage(windowWidth, windowHeight).To(depthImage); !r) { return r; }
			} else {
				return r;
			}*/
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
