#include "JC/Def.h"

#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"
#include "JC/StrDb.h"

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

struct JsonSection {
	Str          json;
	JsonSection* next;
};

struct RegObj {
	Str          jsonMemberName;
	JsonFn*      jsonFn;
	JsonSection* jsonSections;
};

struct Module {
	Str name;
	Span<RegObj> regObjs;
};

static constexpr U64 MaxRegs = 1024;

static RegObj regObjs[MaxRegs];
static U64 regObjsLen;

static constexpr U64 MaxModules = 64;
static Module modules[MaxModules];
static U64 modulesLen;

static constexpr U64 MaxJsonSections = 1024;
static JsonSection jsonSections[MaxJsonSections];
static U64 jsonSectionsLen;

void RegisterModule(Str name, Span<Reg> regs) {
	Assert(modulesLen < MaxModules);
	Assert(regObjsLen + regs.len <= MaxRegs);
	Module* module = &modules[modulesLen++];
	module->name = StrDb::Intern(name);
	module->regObjs.data = regObjs;
	module->regObjs.len  = regs.len;
	regObjsLen += regs.len;
	for (U64 i = 0; i < regs.len; i++) {
		module->regObjs[i] = {
			.jsonMemberName = StrDb::Intern(regs[i].jsonMemberName),
			.jsonFn         = regs[i].jsonFn,
			.jsonSections   = nullptr,
		};
	}
}

Res<> Load(Str path) {
	
}

void Apply(Str module);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def