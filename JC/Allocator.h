#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;
struct VirtualMemoryApi;

//--------------------------------------------------------------------------------------------------

struct AllocatorApi {
	static AllocatorApi* Get();

	virtual void           Init(LogApi* logApi, VirtualMemoryApi* virtualMemoryApi) = 0;
	virtual void           Shutdown() = 0;

	virtual Allocator*     Create(s8 name, Allocator* parent) = 0;
	virtual void           Destroy(Allocator* allocator) = 0;

	virtual TempAllocator* Temp() = 0;
	virtual void           ResetTemp() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC