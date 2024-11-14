#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Panic.h"
#include "JC/Render.h"
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

int main(int argc, const char** argv) {
	AllocatorApi*     allocatorApi     = AllocatorApi::Get();
	LogApi*           logApi           = LogApi::Get();
	PanicApi*         panicApi         = PanicApi::Get();
	RenderApi*        renderApi        = RenderApi::Get();
	TempAllocatorApi* tempAllocatorApi = TempAllocatorApi::Get();
	VirtualMemoryApi* virtualMemoryApi = VirtualMemoryApi::Get();

	allocatorApi->Init();

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

	Allocator* stringsAllocator = allocatorApi->Create("Strings");
	Str::Init(stringsAllocator, virtualMemoryApi);

	Allocator* renderAllocator = allocatorApi->Create("Render");
	if (Res<> r = renderApi->Init(renderAllocator, logApi, tempAllocator); !r) {	
		JC_LOG_ERR(r.err);
		return 1;
	}

	renderApi->Shutdown();

	return 0;
}