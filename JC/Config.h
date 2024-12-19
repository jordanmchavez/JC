#pragma once

#include "JC/Common.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

#define DebugBreakOnErr

u32 GetU32(s8 name);
u64 GetU64(s8 name);

//--------------------------------------------------------------------------------------------------


}	// namespace JC::Config