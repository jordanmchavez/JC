#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;
struct VirtualMemoryApi;

//--------------------------------------------------------------------------------------------------

struct MemLeakReporter {
	virtual void Begin(s8 name, u64 bytes, u32 allocs, u32 children) = 0;
	virtual void Alloc(s8 file, i32 line, u64 bytes, u64 allocs) = 0;
	virtual void Child(s8 name, u64 bytes, u32 allocs) = 0;
	virtual void End() = 0;
};

struct AllocatorApi {
	static AllocatorApi* Get();

	virtual void           Init(VirtualMemoryApi* virtualMemoryApi) = 0;
	virtual void           Shutdown() = 0;

	virtual Allocator*     Create(s8 name, Allocator* parent) = 0;
	virtual void           Destroy(Allocator* allocator) = 0;

	virtual void           SetMemLeakReporter(MemLeakReporter* reporter) = 0;

	virtual TempAllocator* Temp() = 0;
	virtual void           ResetTemp() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC