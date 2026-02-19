#pragma once

#include "JC/Common.h"

namespace JC::Cfg {

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, int argc, char const* const* argv);
Str  GetStr(Str name, Str defVal);
U32  GetU32(Str name, U32 defVal);
void SetStr(Str name, Str val);
void SetU32(Str name, U32 val);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Cfg


