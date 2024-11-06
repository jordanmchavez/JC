#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct SysApi {
	static SysApi* Get();

	virtual void Abort() = 0;
};

//--------------------------------------------------------------------------------------------------

struct DbgApi {
	static DbgApi* Get();

	virtual bool IsPresent() = 0;
	virtual void Print(s8 msg) = 0;
	virtual void Break() = 0;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC