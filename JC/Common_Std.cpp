#include "JC/Common_Std.h"
#include "JC/Common_Assert.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

Str::Str(char const* s, U64 l) { Assert(s || l == 0); data = s; len = l; }

bool operator==(Str s1, Str s2) { return s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len); }
bool operator!=(Str s1, Str s2) { return s1.len != s2.len ||  memcmp(s1.data, s2.data, s1.len); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC