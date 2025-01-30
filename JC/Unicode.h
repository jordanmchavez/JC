#pragma once

#include "JC/Core.h"

namespace JC::Unicode {

//--------------------------------------------------------------------------------------------------

WStrZ Utf8ToWtf16z(Mem::Allocator* allocator, Str s);
Str   Wtf16zToUtf8(Mem::Allocator* allocator, WStrZ s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unicode