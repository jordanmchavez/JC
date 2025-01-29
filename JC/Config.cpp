#pragma once

#include "JC/Config.h"
#include "JC/Map.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

struct ValStr {
	const char* str;
	u64         len;
};

struct Val {
	union {
		u64    u;
		f64    f;
		ValStr s;
	};
};

static Map<Str, Val> vals;

//--------------------------------------------------------------------------------------------------

void Init(Mem::Allocator* allocator) {
	vals.Init(allocator);
}

//--------------------------------------------------------------------------------------------------

u32 GetU32(Str name, u32 def) {
	name; return def;
}

u64 GetU64(Str name, u64 def) {
	name; return def;
}

f32 GetF32(Str name, f32 def) {
	name; return def;
}

f64 GetF64(Str name, f64 def) {
	name; return def;
}

Str GetStr(Str name, Str def) {
	name; return def;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Config