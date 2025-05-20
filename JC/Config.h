#pragma once

#include "JC/Core.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

#define DebugBreakOnErr
#define Render_Debug

void Init(Mem::Allocator* allocator);

U32  GetU32(Str name, U32 def);
U64  GetU64(Str name, U64 def);
F32  GetF32(Str name, F32 def);
F64  GetF64(Str name, F64 def);
Bool GetBool(Str name, Bool def);
Str  GetStr(Str name, Str def);

//--------------------------------------------------------------------------------------------------


}	// namespace JC::Config