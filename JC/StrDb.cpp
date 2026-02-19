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

Str Get(Str s) {
	if (Str* intern = index.FindOrNull(s)) {
		return *intern;
	}
	Str newIntern(Mem::AllocT<char>(mem, s.len), s.len);
	memcpy((char*)newIntern.data, s.data, s.len);
	index.Put(newIntern, newIntern);
	return newIntern;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::StrDb