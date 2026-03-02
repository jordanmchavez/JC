#include "JC/Cfg.h"
#include "JC/Hash.h"
#include "JC/Map.h"

namespace JC::Cfg {

//--------------------------------------------------------------------------------------------------

struct Cfg {
	Str name;
	Str str;
	U32 u32;
};

static constexpr U32 MaxCfgs = 1024;

static Cfg           cfgs[MaxCfgs];
static U32           cfgsLen = 0;
static Map<Str, U32> cfgsIndex;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, int argc, char const* const* argv) {
	argc;argv;
	cfgsIndex.Init(permMem, MaxCfgs);
}

//--------------------------------------------------------------------------------------------------

Str GetStr(Str name, Str defVal) {
	U32* idx = cfgsIndex.FindOrNull(name);
	if (!idx) {
		Assert(cfgsLen < MaxCfgs);
		cfgsIndex.Put(name, cfgsLen);
		cfgs[cfgsLen++] = {
			.name = name,
			.str  = defVal,
		};
		return defVal;
	}
	return cfgs[*idx].str;
}

//--------------------------------------------------------------------------------------------------

U32 GetU32(Str name, U32 defVal) {
	U32* idx = cfgsIndex.FindOrNull(name);
	if (!idx) {
		Assert(cfgsLen < MaxCfgs);
		cfgsIndex.Put(name, cfgsLen);
		cfgs[cfgsLen++] = {
			.name = name,
			.u32  = defVal,
		};
		return defVal;
	}
	return cfgs[*idx].u32;
}

//--------------------------------------------------------------------------------------------------

void SetStr(Str name, Str val) {
	U32* idx = cfgsIndex.FindOrNull(name);
	if (!idx) {
		Assert(cfgsLen < MaxCfgs);
		cfgsIndex.Put(name, cfgsLen);
		cfgs[cfgsLen++] = {
			.name = name,
			.str= val,
		};
	} else {
		cfgs[*idx].str = val;
	}
}

//--------------------------------------------------------------------------------------------------

void SetU32(Str name, U32 val) {
	U32* idx = cfgsIndex.FindOrNull(name);
	if (!idx) {
		Assert(cfgsLen < MaxCfgs);
		cfgsIndex.Put(name, cfgsLen);
		cfgs[cfgsLen++] = {
			.name = name,
			.u32  = val,
		};
	} else {
		cfgs[*idx].u32 = val;
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Cfg