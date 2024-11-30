#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"
#include "JC/Render.h"
#include "JC/UnitTest.h"
#include <stdio.h>
#include "JC/MinimalWindows.h"

using namespace JC;

static TempMem* tempMem = 0;

constexpr s8 FileNameOnly(s8 path) {
	for (const char* i = path.data + path.len - 1; i >= path.data; i--) {
		if (*i == '/' || *i == '\\') {
			return s8(i + 1, path.data + path.len);
		}
	}
	return path;
}

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

	RenderApi* renderApi = RenderApi::Get();
	Mem* renderMem = memApi->CreateScope("render", 0);
	if (Res<> r = renderApi->Init(logApi, renderMem, tempMem); !r) {
		LogErr(r.err);
		return 1;
	}

	renderApi->Shutdown();

	return 0;
}