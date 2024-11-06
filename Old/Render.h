#pragma once

#include "JC_Common.h"

//--------------------------------------------------------------------------------------------------

constexpr JC_ErrCode JC_Render_Err                    = { .ns = "render", .code = 1 };
constexpr JC_ErrCode JC_Render_ErrNoVkDll             = { .ns = "render", .code = 2 };
constexpr JC_ErrCode JC_Render_ErrLayerNotFound       = { .ns = "render", .code = 3 };
constexpr JC_ErrCode JC_Render_ErrInstanceExtNotFound = { .ns = "render", .code = 4 };
constexpr JC_ErrCode JC_Render_ErrNoDevices           = { .ns = "render", .code = 5 };
constexpr JC_ErrCode JC_Render_ErrMem                 = { .ns = "render", .code = 6 };

//--------------------------------------------------------------------------------------------------

struct JC_Render_InitData {
	u32   width;
	u32   height;
	void* sysWindowHandle;
};

JC_Res<> JC_Render_Init(JC_Render_InitData init);
void     JC_Render_Shutdown();
JC_Res<> JC_Render_Frame();


namespace JC {
constexpr ErrCode Render_Err                    = { .ns = "render", .code = 1 };
constexpr ErrCode Render_ErrNoVkDll             = { .ns = "render", .code = 2 };
constexpr ErrCode Render_ErrLayerNotFound       = { .ns = "render", .code = 3 };
constexpr ErrCode Render_ErrInstanceExtNotFound = { .ns = "render", .code = 4 };
constexpr ErrCode Render_ErrNoDevices           = { .ns = "render", .code = 5 };
constexpr ErrCode Render_ErrMem                 = { .ns = "render", .code = 6 };

//--------------------------------------------------------------------------------------------------

namespace Render {
	struct InitData {
		u32   width;
		u32   height;
		void* sysWindowHandle;
	};

	Res<> Init(InitData init);
	void  Shutdown();
	Res<> Frame();
}

}