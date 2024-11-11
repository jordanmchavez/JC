#pragma once

#include "JC/Common.h"

namespace JC {

struct LogApi;

//--------------------------------------------------------------------------------------------------

struct RenderApi {
	static RenderApi* Get();

	virtual Res<> Init(LogApi* logApi, TempAllocator* ta) = 0;
	virtual void  Shutdown() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC