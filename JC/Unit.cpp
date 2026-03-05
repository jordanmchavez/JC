#include "JC/Unit.h"

#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefErr(Unit, DuplicateUnitDef);
DefErr(Unit, UnitDefNotFound);

static constexpr U32 MaxUnitDefs = 1024;
static constexpr U32 MaxUnits    = 64 * 1024;

struct UnitDefJson {
	Str name;
	Str sprite;
	U32 hp;
	U32 move;
};
Json_Begin(UnitDefJson)
	Json_Member("name",   name)
	Json_Member("sprite", sprite)
	Json_Member("hp",     hp)
	Json_Member("move",   move)
Json_End(UnitDefJson)

struct UnitDefsJson {
	Str               atlasJsonPath;
	Span<UnitDefJson> unitDefs;
};
Json_Begin(UnitDefsJson)
	Json_Member("atlasJsonPath", atlasJsonPath)
	Json_Member("unitDefs",      unitDefs)
Json_End(UnitDefsJson)

//--------------------------------------------------------------------------------------------------

static Mem                tempMem;
static UnitDef*           unitDefs;
static U32                unitDefsLen;
static Map<Str, UnitDef*> unitDefMap;
static Unit*              units;
static U32                unitsLen;
static U32                freeUnitIdx;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMemIn) {
	tempMem     = tempMemIn;
	unitDefs    = Mem::AllocT<UnitDef>(permMem, MaxUnitDefs);
	unitDefsLen = 0;
	unitDefMap.Init(permMem, MaxUnitDefs);
	units       = Mem::AllocT<Unit>(permMem, MaxUnits);
	unitsLen    = 0;
	freeUnitIdx = 0;
}

//--------------------------------------------------------------------------------------------------

// TODO: track loaded, avoid duplicates?
Res<> LoadUnitDefsJson(Str unitJsonDefsPath) {
	Span<char> json; TryTo(File::ReadAllZ(unitJsonDefsPath), json);
	UnitDefsJson unitDefsJson; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &unitDefsJson));

	Try(Draw::LoadAtlasJson(unitDefsJson.atlasJsonPath));

	Assert(unitDefsLen + unitDefsJson.unitDefs.len < MaxUnitDefs);
	for (U64 i = 0; i < unitDefsJson.unitDefs.len; i++) {
		UnitDefJson const* const unitDefJson = &unitDefsJson.unitDefs[i];
		UnitDef* const unitDef = &unitDefs[unitDefsLen++];
		unitDef->name = unitDefJson->name;	// already interned
		unitDef->hp   = unitDefJson->hp;
		unitDef->move = unitDefJson->move;
		TryTo(Draw::GetSprite(unitDefJson->sprite), unitDef->sprite);
		unitDef->size = Draw::GetSpriteSize(unitDef->sprite);
		if (unitDefMap.FindOrNull(unitDef->name)) {
			return Err_DuplicateUnitDef("name", unitDef->name);
		}
		unitDefMap.Put(unitDef->name, unitDef);
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<UnitDef const*> GetUnitDef(Str name) {
	UnitDef const* const* const unitDefPP = unitDefMap.FindOrNull(name);
	if (!unitDefPP) {
		return Err_UnitDefNotFound("name", name);
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

void FreeUnit(Unit* unit) {
	Assert(unit >= units && unit < units + unitsLen);
	memset(unit, 0, sizeof(Unit));
	unit->nextFreeIdx = freeUnitIdx;
	freeUnitIdx = (U32)(unit - units);
}

//--------------------------------------------------------------------------------------------------

void Frame(F32 sec) {
	sec;
}

//--------------------------------------------------------------------------------------------------

void Draw(F32 z) {
	z;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit