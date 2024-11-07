#include "JC/Allocator.h"
#include "JC/Log.h"
#include "JC/Sys.h"
#include "JC/UnitTest.h"
#include <stdio.h>

using namespace JC;

static LogApi*           logApi;
static VirtualMemoryApi* virtualMemoryApi;
static AllocatorApi*     allocatorApi;
static TempAllocator*    tempAllocator;

int main() {
	logApi           = LogApi::Get();
	virtualMemoryApi = VirtualMemoryApi::Get();
	allocatorApi     = AllocatorApi::Get();
	tempAllocator    = allocatorApi->Temp();

	allocatorApi->Init(logApi, virtualMemoryApi);

	logApi->AddFn([](s8, i32, LogCategory, s8 msg) {
		fwrite(msg.data, 1, msg.len, stdout);
		fwrite("\n", 1, 1, stdout);
	});
	logApi->AddFn([](s8, i32, LogCategory, s8 msg) {
		Sys::DebuggerPrint(tempAllocator, msg);
	});

	UnitTest::Run(tempAllocator);
	return 0;
}