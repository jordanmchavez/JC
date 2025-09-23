#pragma once

#include "JC/Common.h"

struct Arena;

//--------------------------------------------------------------------------------------------------

Span<wchar_t> Unicode_Utf8ToWtf16z(Arena* a, Str s);
Str           Unicode_Wtf16zToUtf8(Arena* a, Span<wchar_t> s);