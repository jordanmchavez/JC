#pragma once

#include "JC/Core.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

#define JC_DEBUG_BREAK_ON_ERR
#define JC_RENDER_DEBUG

void Init(Allocator* allocator);

U32  GetU32(Str name, U32 def);
U64  GetU64(Str name, U64 def);
F32  GetF32(Str name, F32 def);
F64  GetF64(Str name, F64 def);
Bool GetBool(Str name, Bool def);
Str  GetStr(Str name, Str def);

void SetU32(Str name, U32 u);
void SetU64(Str name, U64 u);
void SetF64(Str name, F64 f);
void SetF32(Str name, F32 f);
void SetBool(Str name, Bool b);
void SetStr(Str name, Str s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Config