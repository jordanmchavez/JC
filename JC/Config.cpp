#pragma once

#include "JC/Config.h"
#include "JC/Map.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

struct ValStr {
	const char* str;
	U64         len;
};

struct Val {
	union {
		U64    u;
		F64    f;
		ValStr s;
	};
};

static Map<Str, Val> vals;

//--------------------------------------------------------------------------------------------------

void Init(Mem::Allocator* allocator) {
	vals.Init(allocator);
}

//--------------------------------------------------------------------------------------------------

U32 GetU32(Str name, U32 def) {
	name; return def;
}

U64 GetU64(Str name, U64 def) {
	name; return def;
}

F32 GetF32(Str name, F32 def) {
	name; return def;
}

F64 GetF64(Str name, F64 def) {
	name; return def;
}

Str GetStr(Str name, Str def) {
	name; return def;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Config