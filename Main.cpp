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

struct MemLeakReporterObj : MemLeakReporter {
	void Begin(s8 name, u64 bytes, u32 allocs, u32 children) override {name;bytes;allocs;children;}
	void Alloc(SrcLoc sl, u64 bytes, u64 allocs) override {sl;bytes;allocs;}
	void Child(s8 name, u64 bytes, u32 allocs) override {name;bytes;allocs;}
	void End() override {}
};

int main(int argc, const char** argv) {
	//RenderApi*        renderApi        = RenderApi::Get();

	SetPanicFn(MyPanicFn);

	MemLeakReporterObj memLeakReporterObj;

	MemApi* memApi = MemApi::Get();
	memApi->Init((u64)16 * 1024 * 1024 * 1024, (u64)4 * 1024 * 1024 * 1024, &memLeakReporterObj);
	tempMem = memApi->Temp();

	LogApi* logApi = LogApi::Get();
	logApi->Init(tempMem);

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

	//if (Res<> r = Render::Init(&scratch); !r) {	
	//	LogErr(scratch, r.err);
	//	return 1;
	//}

	//Render::Shutdown();

	return 0;
}