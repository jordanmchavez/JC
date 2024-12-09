#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/Render.h"
#include "JC/Unicode.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"
#include <stdio.h>
#include "JC/MinimalWindows.h"

using namespace JC;

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

static MemApi*         memApi;
static TempMem*        tempMem;
static LogApi*         logApi;
static Log*            log;
static LogLeakReporter logLeakReporter;
static RenderApi*      renderApi;

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
			iter = Fmt(iter, end, "expr: {}\n", expr);
		}
		if (fmt.len > 0) {
			iter = VFmt(iter, end, fmt, args);
			iter = Fmt(iter, end, "\n");
		}
		Logf("{}", s8(msg, (u64)(end - msg)));
	});

	if (argc == 2 && argv[1] == s8("test")) {
		UnitTest::Run(log, memApi);
		return Ok();
	}

	EventApi* eventApi = GetEventApi();
	eventApi->Init(log, tempMem);

	DisplayApi* displayApi = GetDisplayApi();
	if (Res<> r = displayApi->Init(log); !r) {
		return r;
	}

	WindowApi* windowApi = GetWindowApi();
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
	renderApi = GetRenderApi();
	WindowPlatformData windowPlatformData = windowApi->GetPlatformData();
	RenderApiInit renderApiInit = {
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

	u64 frame = 0;
	bool exitRequested = false;
	while (!exitRequested) {
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
					//Logf("key {} ({}): {}", EventKeyStr(e->key.key), e->key.key, e->key.down ? "down" : "up");
					break;
			}
		}
		eventApi->ClearEvents();

		if (Res<> r = renderApi->Draw(); !r) {
			Errorf(r.err);
			return r;
		}

		memApi->Frame(frame);
	}

	/*Mem* renderMem = memApi->CreateScope("render", 0);
	if (Res<> r = renderApi->Init(logApi, renderMem, tempMem, &osWindowData); !r) {
		Errorf(r.err);
		return 1;
	}

	MSG msg;
	for (;;) {
		PeekMessageW(&msg, 0, 0, 0, PM_REMOVE);
		if (msg.message == WM_QUIT) {
			break;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		renderApi->Frame().Ignore();
	}

	renderApi->Shutdown();
	*/

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