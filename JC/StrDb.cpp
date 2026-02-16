#include "JC/StrDb.h"
#include "JC/Hash.h"
#include "JC/Map.h"

namespace JC::StrDb {

//--------------------------------------------------------------------------------------------------

static constexpr U64 MaxStrings = 1 * MB;	// bump as needed

static Mem           mem;
static Map<Str, Str> index;

//--------------------------------------------------------------------------------------------------

void Init() {
	mem = Mem::Create(1 * GB);
	index.Init(mem, MaxStrings);
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	Mem::Destroy(mem);
}

//--------------------------------------------------------------------------------------------------

Str Get(const char* s, U32 len) {
	if (Str* intern = index.FindOrNull(s)) {
		return *intern;
	}
	Str newIntern(Mem::AllocT<char>(mem, len), len);
	memcpy((char*)newIntern.data, s, len);
	index.Put(newIntern, newIntern);
	return newIntern;
}

Str Get(const char* begin, const char* end) { return Get(begin, (U32)(end - begin)); }
Str Get(Str s) { return Get(s.data, s.len); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::StrDb