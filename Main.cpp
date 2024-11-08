#include "JC/Allocator.h"
#include "JC/Log.h"
#include "JC/Panic.h"
#include "JC/Sys.h"
#include "JC/UnitTest.h"
#include <stdio.h>

using namespace JC;

static LogApi*           logApi;
static AllocatorApi*     allocatorApi;
static PanicApi*         panicApi;
static VirtualMemoryApi* virtualMemoryApi;

static TempAllocator*    tempAllocator;

int main() {
	allocatorApi     = AllocatorApi::Get();
	logApi           = LogApi::Get();
	panicApi         = PanicApi::Get();
	virtualMemoryApi = VirtualMemoryApi::Get();

	allocatorApi->Init(virtualMemoryApi);
	tempAllocator = allocatorApi->Temp();

	logApi->AddFn([](s8, i32, LogCategory, s8 msg) {
		fwrite(msg.data, 1, msg.len, stdout);
		fwrite("\n", 1, 1, stdout);
	});
	logApi->AddFn([](s8, i32, LogCategory, s8 msg) {
		Sys::DebuggerPrint(tempAllocator, msg);
	});

	panicApi->Init(logApi);
	SetPanicApi(panicApi);

	UnitTest::Run(tempAllocator);

	return 0;
}