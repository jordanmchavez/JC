#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct RenderApi {
	static RenderApi* Get();

	virtual void Init() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC