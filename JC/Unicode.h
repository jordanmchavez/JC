#pragma once

#include "JC/Common.h"

namespace JC::Unicode {

//--------------------------------------------------------------------------------------------------

Span<wchar_t> Utf8ToWtf16z(Mem mem, Str s);
Str           Wtf16zToUtf8(Mem mem, Span<wchar_t> s);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unicode