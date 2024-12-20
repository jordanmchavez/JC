#pragma once

#include "JC/Common.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

#define DebugBreakOnErr
#define RenderDebug

void Init(Arena* perm);

u32  GetU32(s8 name, u32 def);
u64  GetU64(s8 name, u64 def);
f32  GetF32(s8 name, f32 def);
f64  GetF64(s8 name, f64 def);
s8   GetS8(s8 name, s8 def);

//--------------------------------------------------------------------------------------------------


}	// namespace JC::Config