#pragma once

#include "JC/Core.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

#define DebugBreakOnErr
#define RenderDebug

void Init(Mem::Allocator* allocator);

u32  GetU32(Str name, u32 def);
u64  GetU64(Str name, u64 def);
f32  GetF32(Str name, f32 def);
f64  GetF64(Str name, f64 def);
Str  GetStr(Str name, Str def);

//--------------------------------------------------------------------------------------------------


}	// namespace JC::Config