#include "JC/Allocator.h"
#include "JC/Log.h"
#include "JC/Panic.h"
#include "JC/Sys.h"
#include "JC/UnitTest.h"
#include <stdio.h>

using namespace JC;

int main() {
	LogApi*           logApi           = LogApi::Get();
	PanicApi*         panicApi         = PanicApi::Get();
	TempAllocatorApi* tempAllocatorApi = TempAllocatorApi::Get();
	VirtualMemoryApi* virtualMemoryApi = VirtualMemoryApi::Get();

	tempAllocatorApi->Init(virtualMemoryApi);
	logApi->Init(tempAllocatorApi);
	panicApi->Init(logApi);
	SetPanicApi(panicApi);
	UnitTest::Run(logApi, tempAllocatorApi);

	return 0;
}