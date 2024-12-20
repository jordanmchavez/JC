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

static Arena*       perm;
static Map<s8, Val> vals;

void Init(Arena* permIn) {
	perm = permIn;
	vals.Init(perm, 128, 128);
}

//--------------------------------------------------------------------------------------------------

u32 GetU32(s8 name, u32 def) {
	name; return def;
}

u64 GetU64(s8 name, u64 def) {
	name; return def;
}

f32 GetF32(s8 name, f32 def) {
	name; return def;
}

f64 GetF64(s8 name, f64 def) {
	name; return def;
}

s8 GetS8(s8 name, s8 def) {
	name; return def;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Config