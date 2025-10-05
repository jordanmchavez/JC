#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

Mem*  Mem_Create(U64 reserve);
void  Mem_Destroy(Mem* mem);
void* Mem_Alloc(Mem* mem, U64 size, SrcLoc sl = SrcLoc_Here());
bool  Mem_Extend(Mem* mem, void* p, U64 size, SrcLoc sl = SrcLoc_Here());
void  Mem_Reset(Mem* mem);