#include "JC/StrDb.h"
#include "JC/Common_Mem.h"
#include "JC/Hash.h"
#include "JC/Map.h"

namespace JC::StrDb {

//--------------------------------------------------------------------------------------------------

static constexpr U64 MaxStrings = 8 * KB;	// bump as needed

static Mem::Mem      permMem;
static Map<Str, Str> index;

//--------------------------------------------------------------------------------------------------

void Init() {
	permMem = Mem::Create(1 * GB);
	index.Init(permMem, MaxStrings);
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	Mem::Destroy(permMem);
}

//--------------------------------------------------------------------------------------------------

Str Get(Str s) {
	if (Str* intern = index.FindOrNull(s)) {
		return *intern;
	}
	Str newIntern(Mem::AllocT<char>(permMem, s.len), s.len);
	memcpy((char*)newIntern.data, s.data, s.len);
	index.Put(newIntern, newIntern);
	return newIntern;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::StrDb