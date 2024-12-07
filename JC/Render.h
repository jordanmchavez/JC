#pragma once

#include "JC/Common.h"

namespace JC {

struct Log;
struct Mem;
struct TempMem;

//--------------------------------------------------------------------------------------------------

struct RenderApiInit {
	Log*     log              = 0;
	Mem*     mem              = 0;
	TempMem* tempMem          = 0;
	u32      width            = 0;
	u32      height           = 0;
	void*    osWindowHandle   = 0;
	#if defined Os_Windows
	void*    osInstanceHandle = 0;
	#endif	// Os_Windows
};

struct RenderApi {
	static constexpr ErrCode Err_LayerNotFound     = { .ns = "ren", .code = 1 };
	static constexpr ErrCode Err_ExtensionNotFound = { .ns = "ren", .code = 2 };
	static constexpr ErrCode Err_NoSuitableDevice  = { .ns = "ren", .code = 3 };
	
	virtual Res<> Init(const RenderApiInit* init) = 0;
	virtual void  Shutdown() = 0;
	virtual Res<> Frame() = 0;
};

RenderApi* GetRenderApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC