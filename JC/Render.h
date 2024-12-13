#pragma once

#include "JC/Common.h"

namespace JC {

struct FileApi;
struct Log;
struct Mem;
struct Mat4;
struct TempMem;
struct WindowPlatformData;

//--------------------------------------------------------------------------------------------------

struct RenderApiInit {
	FileApi*            fileApi            = 0;
	Log*                log                = 0;
	Mem*                mem                = 0;
	TempMem*            tempMem            = 0;
	u32                 width              = 0;
	u32                 height             = 0;
	WindowPlatformData* windowPlatformData = {};
};

struct RenderApi {
	static constexpr ErrCode Err_Version  = { .ns = "render", .code = 1 };
	static constexpr ErrCode Err_NoLayer  = { .ns = "render", .code = 2 };
	static constexpr ErrCode Err_NoDevice = { .ns = "render", .code = 3 };
	static constexpr ErrCode Err_NoMem    = { .ns = "render", .code = 4 };

	virtual Res<> Init(const RenderApiInit* init) = 0;
	virtual void  Shutdown() = 0;
	virtual Res<> Draw(const Mat4* view, const Mat4* proj) = 0;
};

RenderApi* GetRenderApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC