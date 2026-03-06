#include "JC/Unit.h"

#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefErr(Unit, DuplicateDef);
DefErr(Unit, DefNotFound);

static constexpr U32 MaxDefs  = 1024;
static constexpr U32 MaxUnits = 64 * 1024;

// We don't need a `Def` because it's the exact same as `Def`
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

static Mem                  tempMem;
static Def*                 defs;
static U32                  defsLen;
static Map<Str, Def const*> defsMap;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMemIn) {
	tempMem = tempMemIn;
	defs    = Mem::AllocT<Def>(permMem, MaxDefs);
	defsLen = 1;	// reserve 0 for invalid
	defsMap.Init(permMem, MaxDefs);
}

//--------------------------------------------------------------------------------------------------

// TODO: track loaded, avoid duplicates?
Res<> LoadDefs(Str path) {
	Span<char> json; TryTo(File::ReadAllZ(path), json);
	DefsJson defsJson; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &defsJson));

	Try(Draw::LoadAtlas(defsJson.atlasPath));

	Assert(defsLen + defsJson.defs.len < MaxDefs);
	for (U64 i = 0; i < defsJson.defs.len; i++) {
		DefJson const* const unitDefJson = &defsJson.defs[i];
		Def* const def = &defs[defsLen++];
		def->name = unitDefJson->name;	// already interned
		def->hp   = unitDefJson->hp;
		def->move = unitDefJson->move;
		TryTo(Draw::GetSprite(unitDefJson->sprite), def->sprite);
		def->size = Draw::GetSpriteSize(def->sprite);
		if (defsMap.FindOrNull(def->name)) {
			return Err_DuplicateDef("name", def->name);
		}
		defsMap.Put(def->name, def);
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Unit> GetUnit(Str name) {
	Def const* const* const defDataPP = defsMap.FindOrNull(name);
	if (!defDataPP ) {
		return Err_DefNotFound("name", name);
	}
	return Unit { .handle = (U64)(*defDataPP - defs) };
}

//--------------------------------------------------------------------------------------------------

Def const* GetDef(Unit unit) {
	Assert(unit.handle > 0 && unit.handle < defsLen);
	return &defs[unit.handle];
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit