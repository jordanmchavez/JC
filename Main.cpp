#include "JC/Err.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/Render.h"
#include "JC/Unicode.h"
#include "JC/UnitTest.h"
#include <stdio.h>
#include "JC/MinimalWindows.h"

using namespace JC;

static TempMem* tempMem = 0;

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

[[noreturn]] void MyPanicFn(s8 file, i32 line, s8 expr, s8 fmt, Args args) {
	char msg[1024];
	char* end = msg + sizeof(msg) - 1;
	end = Fmt(msg, end, "***PANIC***\n");
	end = Fmt(msg, end, "{}({})\n", file, line);
	end = Fmt(msg, end, "expr: {}\n", expr);
	end = Fmt(msg, end, "msg:  ");
	end = VFmt(msg, end, fmt, args);
	end = Fmt(msg, end, "\n");
	*end = '\0';
	fwrite(msg, 1, end - msg, stdout);
	if (Sys::IsDebuggerPresent()) {
		Sys::DebuggerPrint(msg);
		Sys_DebuggerBreak();
	}
	Sys::Abort();
}

//--------------------------------------------------------------------------------------------------

struct LogLeakReporter : MemLeakReporter {
	Log* log = 0;

	void LeakedScope(s8 name, u64 leakedBytes, u32 leakedAllocs, u32 leakedChildren) override {
		LogErrorf("Leaked scope: name={}, leakedBytes={}, leakedAllocs={}, leakedChildren={}", name, leakedBytes, leakedAllocs, leakedChildren);
	}

	void LeakedAlloc(SrcLoc sl, u64 leakedBytes, u32 leakedAllocs) override {
		LogErrorf("  {}({}): leakedBytes={}, leakedAllocs={}", sl.file, sl.line, leakedBytes, leakedAllocs);
	}

	void LeakedChild(s8 name, u64 leakedBytes, u32 leakedAllocs) override {
		LogErrorf("  Leaked child: name={}, leakedBytes={}, leakedAllocs={}", name, leakedBytes, leakedAllocs);
	}
};

//--------------------------------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
		case WM_CLOSE:
			PostQuitMessage(0);
			break;
		default:
			break;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//--------------------------------------------------------------------------------------------------

int main(int argc, const char** argv) {
	//RenderApi*        renderApi        = RenderApi::Get();

	SetPanicFn(MyPanicFn);

	MemApi* memApi = GetMemApi();
	memApi->Init();
	tempMem = memApi->Temp();

	LogApi* logApi = GetLogApi();
	logApi->Init(tempMem);
	Log* log = logApi->GetLog();

	LogLeakReporter logLeakReporter;
	logLeakReporter.log = log;
	memApi->SetLeakReporter(&logLeakReporter);

	if (argc == 2 && argv[1] == s8("test")) {
		logApi->AddFn([](const char* msg, u64 len) {
			fwrite(msg, 1, len, stdout);
			if (Sys::IsDebuggerPresent()) {
				Sys::DebuggerPrint(msg);
			}
		});

		return UnitTest::Run(tempMem, logApi) ? 0 : 1;
	}

	logApi->AddFn([](const char* msg, u64 len) {
		fwrite(msg, 1, len, stdout);
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(msg);
		}
	});

	/*Mem* renderMem = memApi->CreateScope("render", 0);
	if (Res<> r = renderApi->Init(logApi, renderMem, tempMem, &osWindowData); !r) {
		LogErr(r.err);
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
	return 0;
}