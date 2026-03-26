#include "JC/StrDb.h"

#include "JC/Hash.h"
#include "JC/Map.h"

namespace JC::StrDb {

//--------------------------------------------------------------------------------------------------

static constexpr U64 MaxStrings = 1 * MB;	// bump as needed

static constexpr char const* Empty = "";

static Mem           mem;
static Map<Str, Str> index;

//--------------------------------------------------------------------------------------------------

void Init() {
	mem = Mem::Create(1 * GB);
	index.Init(mem, MaxStrings);
}

//--------------------------------------------------------------------------------------------------

Str Intern(Str s) {
	if (s.len == 0) { return Str(Empty, 0); }
	Str str = index.FindOrZero(s);
	if (str.len) { return str; }
	str.data = (char*)Mem::Alloc(mem, s.len);
	str.len  = s.len;
	memcpy((char*)str.data, s.data, s.len);
	index.Put(s, str);
	return str;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::StrDb