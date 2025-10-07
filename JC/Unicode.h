#pragma once

#include "JC/Common_Mem.h"
#include "JC/Common_Span.h"


namespace JC::Unicode {

//--------------------------------------------------------------------------------------------------

Span<wchar_t> Utf8ToWtf16z(Mem::Mem* mem, Str s);
Str           Wtf16zToUtf8(Mem::Mem* mem, Span<wchar_t> s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unicode