#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

void Rng_Seed(U64 s[2]);
U32  Rng_NextU32();
U64  Rng_NextU64();
F32  Rng_NextF32();
F64  Rng_NextF64();