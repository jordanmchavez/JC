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

template <class... A>
Err* _MakeWinErr(SrcLoc sl, s8 fn, A... args) {
	const DWORD code = GetLastError();
	const ErrCode ec = { .ns = "win", .code = (u64)code };
	wchar_t* desc = nullptr;
	DWORD descLen = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&desc,
		0,
		nullptr
	);
	if (descLen == 0) {
		return _MakeErr(0, sl, ec, "fn", fn, args...);
	}

	while (descLen > 0 && desc[descLen] == L'\n' || desc[descLen] == L'\r' || desc[descLen] == L'.') {
		--descLen;
	}
	s8 msg = Unicode::Wtf16zToUtf8(tempMem, s16z(desc, descLen));
	LocalFree(desc);

	return _MakeErr(0, sl, ec, "fn", fn, "desc", desc, args...);
}

#define MakeWinErr(fn, ...) \
	_MakeWinErr(SrcLoc::Here(), #fn, ##__VA_ARGS__)

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
	LogApi* logApi = 0;

	void LeakedScope(s8 name, u64 leakedBytes, u32 leakedAllocs, u32 leakedChildren) override {
		LogError("Leaked scope: name={}, leakedBytes={}, leakedAllocs={}, leakedChildren={}", name, leakedBytes, leakedAllocs, leakedChildren);
	}

	void LeakedAlloc(SrcLoc sl, u64 leakedBytes, u32 leakedAllocs) override {
		LogError("  {}({}): leakedBytes={}, leakedAllocs={}", sl.file, sl.line, leakedBytes, leakedAllocs);
	}

	void LeakedChild(s8 name, u64 leakedBytes, u32 leakedAllocs) override {
		LogError("  Leaked child: name={}, leakedBytes={}, leakedAllocs={}", name, leakedBytes, leakedAllocs);
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

	MemApi* memApi = MemApi::Get();
	memApi->Init();
	tempMem = memApi->Temp();

	LogApi* logApi = LogApi::Get();
	logApi->Init(tempMem);

	LogLeakReporter logLeakReporter;
	logLeakReporter.logApi = logApi;
	memApi->SetLeakReporter(&logLeakReporter);

	if (argc == 2 && argv[1] == s8("test")) {
		return UnitTest::Run(tempMem, logApi) ? 0 : 1;
	}

	logApi->AddFn([](SrcLoc sl, LogCategory category, const char* msg, u64) {
		s8 fullMsg = Fmt(
			tempMem,
			"{}{}({}): {}",
			category == LogCategory::Error ? "!!! " : "",
			sl.file,
			sl.line,
			msg
		);
		fwrite(fullMsg.data, 1, fullMsg.len, stdout);
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(msg);
		}
	});

	const HINSTANCE hinstance = GetModuleHandle(0);
	constexpr const wchar_t* classNameW = L"JC";
	WNDCLASSEXW wndClassExW = {
		.cbSize        = sizeof(wndClassExW),
		.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW,
		.lpfnWndProc   = WndProc,
		.cbClsExtra    = 0,
		.cbWndExtra    = 0,
		.hInstance     = hinstance,
		.hIcon         = 0,
		.hCursor       = 0,
		.hbrBackground = 0,
		.lpszMenuName  = 0,
		.lpszClassName = classNameW,
		.hIconSm       = 0,
	};
	if (!RegisterClassExW(&wndClassExW)) {
		LogErr(MakeWinErr(RegisterClassExW));
		return 1;
	}

	HWND hwnd = CreateWindowExW(
		0,
		classNameW,
		L"JC",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		100, 100,
		800, 600,
		0,
		0,
		hinstance,
		0
	);
	if (!hwnd) {
		LogErr(MakeWinErr(CreateWindowExW));
		return 1;
	}

	OsWindowData osWindowData = {
		.width     = 800,
		.height    = 600,
		.hinstance = hinstance,
		.hwnd      = hwnd,
	};

	RenderApi* renderApi = RenderApi::Get();
	Mem* renderMem = memApi->CreateScope("render", 0);
	if (Res<> r = renderApi->Init(logApi, renderMem, tempMem, &osWindowData); !r) {
		LogErr(r.err);
		return 1;
	}
	/*
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
	*/
	renderApi->Shutdown();

	return 0;
}