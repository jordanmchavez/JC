#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

namespace Render {
	static constexpr ErrCode Err_LayerNotFound     = { .ns = "ren", .code = 1 };
	static constexpr ErrCode Err_ExtensionNotFound = { .ns = "ren", .code = 2 };

	Res<> Init(Mem* perm, Mem* res);
	void  Shutdown();
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC