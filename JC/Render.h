#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;
struct Mem;
struct TempMem;

//--------------------------------------------------------------------------------------------------

struct RenderApi {
	static constexpr ErrCode Err_LayerNotFound     = { .ns = "ren", .code = 1 };
	static constexpr ErrCode Err_ExtensionNotFound = { .ns = "ren", .code = 2 };

	static RenderApi* Get();

	virtual Res<> Init(LogApi* logApi, Mem* mem, TempMem* tempMem) = 0;
	virtual void  Shutdown() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC