#include "JC/Unit.h"

#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/HandlePool.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefErr(Unit, DuplicateDef);
DefErr(Unit, DefNotFound);

static constexpr U32 MaxDefs  = 1024;
static constexpr U32 MaxUnits = 64 * 1024;

struct DefObj {
	DefData defData;
};

struct UnitObj {
};

struct DefJson {
	Str name;
	Str sprite;
	U32 hp;
	U32 move;
};
Json_Begin(DefJson)
	Json_Member("name",   name)
	Json_Member("sprite", sprite)
	Json_Member("hp",     hp)
	Json_Member("move",   move)
Json_End(DefJson)

struct DefsJson {
	Str           atlasPath;
	Span<DefJson> defs;
};
Json_Begin(DefsJson)
	Json_Member("atlasPath", atlasPath)
	Json_Member("defs",      defs)
Json_End(DefsJson)

//--------------------------------------------------------------------------------------------------

static Mem                       tempMem;
static DefObj*                   defObjs;
static U32                       defObjsLen;
static Map<Str, Def*>            defMap;
static HandlePool<Unit, UnitObj> units;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMemIn) {
	tempMem    = tempMemIn;
	defObjs    = Mem::AllocT<DefObj>(permMem, MaxDefs);
	defObjsLen = 0;
	units.Init(permMem, MaxUnits);
	defMap.Init(permMem, MaxDefs);
}

//--------------------------------------------------------------------------------------------------

// TODO: track loaded, avoid duplicates?
Res<> LoadDefs(Str path) {
	Span<char> json; TryTo(File::ReadAllZ(path), json);
	DefsJson defsJson; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &defsJson));

	Try(Draw::LoadAtlas(defsJson.atlasPath));

	Assert(defObjsLen + defsJson.defs.len < MaxDefs);
	for (U64 i = 0; i < defsJson.defs.len; i++) {
		DefJson const* const unitDefJson = &defsJson.defs[i];
		DefObj* const defObj = &defObjs[defObjsLen++];
		defObj->name = unitDefJson->name;	// already interned
		defObj->hp   = unitDefJson->hp;
		defObj->move = unitDefJson->move;
		TryTo(Draw::GetSprite(unitDefJson->sprite), defObj->sprite);
		defObj->size = Draw::GetSpriteSize(defObj->sprite);
		if (unitDefMap.FindOrNull(defObj->name)) {
			return Err_DuplicateDef("name", defObj->name);
		}
		unitDefMap.Put(defObj->name, defObj);
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Def const*> GetDef(Str name) {
	Def const* const* const unitDefPP = unitDefMap.FindOrNull(name);
	if (!unitDefPP) {
		return Err_DefNotFound("name", name);
	}
	return *unitDefPP;
}

//--------------------------------------------------------------------------------------------------

Unit* AllocUnit() {
	if (freeUnitIdx) {
		Unit* const unit = &units[freeUnitIdx];
		freeUnitIdx = unit->nextFreeIdx;
		return unit;
	}
	Assert(unitsLen < MaxUnits);
	return &units[unitsLen++];
}

//--------------------------------------------------------------------------------------------------

void DestroyUnit(Unit unit) {
	units.Free(unit);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit