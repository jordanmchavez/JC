#pragma once

#include "JC/Common.h"

namespace JC::StrDb {

//--------------------------------------------------------------------------------------------------

void Init();
Str  Get(const char* s, U32 len);
Str  Get(const char* begin, const char* end);
Str  Get(Str s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::StrDb