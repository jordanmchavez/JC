#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct MemScope { u64 opaque = 0; };

struct MemTraceApi {
	static MemTraceApi* Get();

	virtual void     Init() = 0;
	virtual MemScope CreateScope(s8 name, MemScope parent) = 0;
	virtual void     DestroyScope(MemScope scope) = 0;
	virtual void     PermAlloc(u64 size) = 0;
	virtual void     AddTrace(MemScope scope, void* ptr, u64 size, SrcLoc sl) = 0;
	virtual void     RemoveTrace(MemScope scope, void* ptr) = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC