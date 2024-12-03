#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;
struct Mem;
struct TempMem;

//--------------------------------------------------------------------------------------------------

struct OsWindowData {
	u32 width = 0;
	u32 height = 0;
	#if defined Os_Windows
		void* hinstance = 0;
		void* hwnd      = 0;
	#else	// Os_
		#error("Unsupported OS");
	#endif	// Os_
};

namespace RenderApiCfgFlags {
	constexpr u64 Debug = (u64)1 << 0;
};

struct RenderInit {
	u64                 flags        = 0;
	LogApi*             logApi       = 0;
	Mem*                mem          = 0;
	TempMem*            tempMem      = 0;
	const OsWindowData* osWindowData = 0;
};

struct RenderApi {
	static constexpr ErrCode Err_LayerNotFound     = { .ns = "ren", .code = 1 };
	static constexpr ErrCode Err_ExtensionNotFound = { .ns = "ren", .code = 2 };
	static constexpr ErrCode Err_NoSuitableDevice  = { .ns = "ren", .code = 3 };
	
	static RenderApi* Get();

	virtual Res<> Init(const RenderInit * renderInitInfo) = 0;
	virtual void  Shutdown() = 0;
	virtual Res<> Frame() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC