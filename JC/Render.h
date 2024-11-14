#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;

//--------------------------------------------------------------------------------------------------

struct RenderApi {
	static RenderApi* Get();

	static constexpr ErrCode Err_LayerNotFound     = { .ns = "ren", .code = 1 };
	static constexpr ErrCode Err_ExtensionNotFound = { .ns = "ren", .code = 2 };

	virtual Res<> Init(Allocator* allocator, LogApi* logApi, TempAllocator* tempAllocator) = 0;
	virtual void  Shutdown() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC