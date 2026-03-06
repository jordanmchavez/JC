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

// We don't need a `DefData` because it's the exact same as `DefData`
// Same for `UnitData` / `UnitObj`

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

static Mem                        tempMem;
static DefData*                   defDatas;
static U32                        defDatasLen;
static Map<Str, DefData const*>   defDatasMap;
static HandlePool<Unit, Data>     units;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMemIn) {
	tempMem    = tempMemIn;
	defDatas    = Mem::AllocT<DefData>(permMem, MaxDefs);
	defDatasLen = 1;	// reserve 0 for invalid
	defDatasMap.Init(permMem, MaxDefs);
	units.Init(permMem, MaxUnits);
}

//--------------------------------------------------------------------------------------------------

// TODO: track loaded, avoid duplicates?
Res<> LoadDefs(Str path) {
	Span<char> json; TryTo(File::ReadAllZ(path), json);
	DefsJson defsJson; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &defsJson));

	Try(Draw::LoadAtlas(defsJson.atlasPath));

	Assert(defDatasLen + defsJson.defs.len < MaxDefs);
	for (U64 i = 0; i < defsJson.defs.len; i++) {
		DefJson const* const unitDefJson = &defsJson.defs[i];
		DefData* const defData = &defDatas[defDatasLen++];
		defData->name = unitDefJson->name;	// already interned
		defData->hp   = unitDefJson->hp;
		defData->move = unitDefJson->move;
		TryTo(Draw::GetSprite(unitDefJson->sprite), defData->sprite);
		defData->size = Draw::GetSpriteSize(defData->sprite);
		if (defDatasMap.FindOrNull(defData->name)) {
			return Err_DuplicateDef("name", defData->name);
		}
		defDatasMap.Put(defData->name, defData);
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Def> GetDef(Str name) {
	DefData const* const* const defDataPP = defDatasMap.FindOrNull(name);
	if (!defDataPP ) {
		return Err_DefNotFound("name", name);
	}
	return Def { .handle = (U64)(*defDataPP - defDatas) };
}

//--------------------------------------------------------------------------------------------------

Data* CreateUnit(Def def, U32 side, Vec2 pos) {
	Assert(def.handle > 0 && def.handle < defDatasLen);
	DefData const* const defData = &defDatas[def.handle];

	auto entry = units.Alloc();

	Data* const data = &entry->obj;
	data->defData = defData;
	data->unit    = entry->Handle();
	data->side    = side;
	data->pos     = pos;
	data->hp      = defData->hp;
	data->move    = defData->move;

	return data;
}

//--------------------------------------------------------------------------------------------------

void DestroyUnit(Unit unit) {
	units.Free(unit);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit