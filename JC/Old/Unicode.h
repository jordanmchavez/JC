#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

Span<wchar_t> Unicode_Utf8ToWtf16z(Mem* mem, Str s);
Str           Unicode_Wtf16zToUtf8(Mem* mem, Span<wchar_t> s);