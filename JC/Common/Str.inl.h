#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

constexpr Str::Str(const char* s) {
	data = s;
	len = ConstExprStrLen(s);
}

constexpr Str::Str(const char* d, u64 l) {
	Assert(d || !l);
	data = d;
	len = l;
}

constexpr Str::Str(const char* b, const char* e) {
	Assert(b <= e);
	data = b;
	len = e - b;
}

constexpr Str& Str::operator=(const char* s) {
	data = s;
	len = ConstExprStrLen(s);
	return *this;
}

constexpr char Str::operator[](u64 i) const {
	Assert(i < len);
	return data[i];
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC