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

static Array<Cfg>     cfgs;
static Map<Str, Cfg*> cfgsMap;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, int argc, char const* const* argv) {
	argc;argv;
	cfgs.Init(permMem, MaxCfgs);
	cfgsMap.Init(permMem, MaxCfgs);
}

//--------------------------------------------------------------------------------------------------

Str GetStr(Str name, Str defVal) {
	Cfg* cfg = cfgsMap.FindOrZero(name);
	if (!cfg) {
		cfg = cfgs.Add();
		cfg->name = name;
		cfg->str  = defVal;
		cfgsMap.Put(name, cfg);
		return defVal;
	}
	return cfg->str;
}

//--------------------------------------------------------------------------------------------------

U32 GetU32(Str name, U32 defVal) {
	Cfg* cfg = cfgsMap.FindOrZero(name);
	if (!cfg) {
		cfg = cfgs.Add();
		cfg->name = name;
		cfg->u32  = defVal;
		cfgsMap.Put(name, cfg);
		return defVal;
	}
	return cfg->u32;
}

//--------------------------------------------------------------------------------------------------

void SetStr(Str name, Str val) {
	Cfg* cfg = cfgsMap.FindOrZero(name);
	if (!cfg) {
		cfg = cfgs.Add();
		cfg->name = name;
		cfg->str  = val;
		cfgsMap.Put(name, cfg);
	} else {
		cfg->str = val;
	}
}

//--------------------------------------------------------------------------------------------------

void SetU32(Str name, U32 val) {
	Cfg* cfg = cfgsMap.FindOrZero(name);
	if (!cfg) {
		cfg = cfgs.Add();
		cfg->name = name;
		cfg->u32  = val;
		cfgsMap.Put(name, cfg);
	} else {
		cfg->u32 = val;
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Cfg