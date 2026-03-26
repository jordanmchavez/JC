#pragma once

#include "JC/Common.h"

namespace JC::Battle::Map {

//--------------------------------------------------------------------------------------------------

void  Init(Mem permMem, Mem tempMemIn);
Res<> Load(Str path);
void  GenerateRandomMap(U32 cols, U32 rows);
void  Draw(F32 z);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle::Map