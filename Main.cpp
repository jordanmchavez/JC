#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Panic.h"
#include "JC/Render_Vk.h"
#include "JC/Sys.h"
#include "JC/UnitTest.h"
#include <stdio.h>
#include "JC/MinimalWindows.h"

using namespace JC;

static TempAllocator* tempAllocator = nullptr;

constexpr s8 FileNameOnly(s8 path) {
	for (const char* i = path.data + path.len - 1; i >= path.data; i--) {
		if (*i == '/' || *i == '\\') {
			return s8(i + 1, path.data + path.len);
		}
	}
	return path;
}

s8 MakeErrStr(TempAllocator* ta, Err* err) {
	Array<char> buf;
	buf.Init(ta);
	for (Err* e = err; e; e = e->prev) {
		Fmt(&buf, "{}-{}:", e->ec.ns, e->ec.code);
	}
	buf[buf.len - 1] = '\n';
	for (Err* e = err; e; e = e->prev) {
		Fmt(&buf, "  {}({}): {}-{}\n", e->file, e->line, e->ec.ns, e->ec.code);
		for (u32 i = 0; i < e->argsLen; i++) {
			Fmt(&buf, "    '{}' = {}\n", e->args[i].name, e->args[i].arg);
		}
	}
	return s8(buf.data, buf.len);
}

int main(int argc, const char** argv) {
	LogApi*           logApi           = LogApi::Get();
	PanicApi*         panicApi         = PanicApi::Get();
	TempAllocatorApi* tempAllocatorApi = TempAllocatorApi::Get();
	VirtualMemoryApi* virtualMemoryApi = VirtualMemoryApi::Get();

	tempAllocatorApi->Init(virtualMemoryApi);
	logApi->Init(tempAllocatorApi);
	panicApi->Init(logApi);
	SetPanicApi(panicApi);

	if (argc == 2 && argv[1] == s8("test")) {
		return UnitTest::Run(logApi, tempAllocatorApi) ? 0 : 1;
	}

	tempAllocator = tempAllocatorApi->Create();

	logApi->AddFn([](s8 file, i32 line, LogCategory category, s8 msg) {
		s8 fullMsg = Fmt(
			tempAllocator,
			"{}{}({}): {}",
			category == LogCategory::Error ? "!!! " : "",
			file,
			line,
			msg
		);
		fwrite(fullMsg.data, 1, fullMsg.len, stdout);
		if (Sys::IsDebuggerPresent()) {
			Sys::DebuggerPrint(tempAllocator, fullMsg);
		}
	});

	Err* e1 = JC_ERR(tempAllocator, ErrCode { .ns = "foo", .code = 1 }, "arg1", "val1", "arg2", 123, "arg3", 4.56);
	Err* e2 = JC_ERR(tempAllocator, ErrCode { .ns = "bar", .code = 2 });
	Err* e3 = JC_ERR(tempAllocator, ErrCode { .ns = "baz", .code = 3 }, "arg4", "bar", "arg5", 80085);
	e1->prev = e2;
	e2->prev = e3;
	JC_LOG_ERROR("{}", MakeErrStr(tempAllocator, e1));

	if (Res<> r = RenderVk::LoadRootFns(tempAllocator); !r) {	
		JC_LOG_ERROR("{}", MakeErrStr(tempAllocator, r.err));
	}
	RenderVk::FreeFns();

	return 0;
}