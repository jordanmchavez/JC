#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Str {
	const char* data = "";
	u64         len  = 0;

	constexpr Str() = default;
	constexpr Str(const Str& s) = default;
	constexpr Str(const char* s);
	constexpr Str(const char* d, u64 l);
	constexpr Str(const char* b, const char* e);

	constexpr Str& operator=(const Str& s) = default;
	constexpr Str& operator=(const char* s);

	constexpr char operator[](u64 i) const;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC