#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

u64 Hash(void const* data, u64 len);

u64 Hash(s8 s)          { return Hash(s.data, s.len); }
u64 Hash(const void* p) { return Hash(&p, sizeof(p)); }
u64 Hash(i64 i)         { return Hash(&i, sizeof(i)); }
u64 Hash(u64 u)         { return Hash(&u, sizeof(u)); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC