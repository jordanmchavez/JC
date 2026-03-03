#include "JC/BattleMap.h"

#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Json.h"

namespace JC::BMap {

//--------------------------------------------------------------------------------------------------

static constexpr U32 HexSize     = 32;
static constexpr U32 MaxTerrains = 128;
static constexpr U32 MaxHexes    = 4096;	// 64x64

struct TerrainObj {
	Str          name;
	Draw::Sprite sprite;
	Vec4         color;
	U32          moveCost;
};

struct HexObj {
	TerrainObj const* terrainObj;
	HexPos            pos;
};

static Mem          tempMem;
static U8           hexLut[(HexSize / 2) * (HexSize * 3 / 8)];
static TerrainObj*  terrainObjs;
static U32          terrainObjsLen;
static HexObj*      hexObjs;
static U32          hexObjsLen;
static Draw::Sprite highlightedHexSprite;
static Vec4         highlightedHexColor;
static HexObj*      highlightedHexObj;
static Draw::Sprite selectedHexSprite;
static Vec4         selectedHexColor;
static HexObj*      selectedHexObj;

//--------------------------------------------------------------------------------------------------

static HexPos ScreenPosToHexPos(Vec2 p) {
	I32 iy = (I32)p.y;
	I32 row = iy / (HexSize * 3 / 4);
	const U8 parity = (row & 1);

	I32 ix = (I32)p.x - parity * (HexSize / 2);
	I32 col = ix / HexSize;
	if (ix < 0 || iy < 0) {
		return { .col = -1, .row = -1 };
	}

	I32 lx = ix - (col * HexSize);
	I32 ly = iy - (row * (HexSize * 3 / 4));
	Assert(lx >= 0 && ly >= 0);

	const U8 l = (hexLut[(ly / 2) * (HexSize / 2) + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;
	switch (l) {
		case 1: col -= 1 * (1 - parity); row -= 1; break;
		case 2: col += 1 * parity;       row -= 1; break;
	}
	return { .col = (I16)col, .row = (I16)row };
}


//--------------------------------------------------------------------------------------------------

static void InitLut() {
	memset(hexLut, 0, sizeof(hexLut));
	U8 start = (HexSize / 2) - 1;
	U8 end   = HexSize / 2;
	for (U8 y = 0; y < HexSize / 4; y++) {
		for (U8 x = 0; x < start; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (1 << ((x & 1) * 4));
		}
		for (U8 x = end + 1; x < HexSize; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (2 << ((x & 1) * 4));
		}
		start -= 2;
		end   += 2;
	}
}

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMemIn) {
	tempMem        = tempMemIn;
	terrainObjs    = Mem::AllocT<TerrainObj>(permMem, MaxTerrains);
	terrainObjsLen = 1;	// reserve 0 for invalid
	hexObjs        = Mem::AllocT<HexObj>    (permMem, MaxHexes);
	hexObjsLen     = 1;	// reserve 0 for invalid
	InitLut();
}

//--------------------------------------------------------------------------------------------------

struct ColorDef { F32 r, g, b, a; };
Json_Begin(ColorDef)
	Json_Member("r", r)
	Json_Member("g", g)
	Json_Member("b", b)
	Json_Member("a", a)
Json_End(ColorDef)

struct SpecialMoveCostDef {
	Str ability;
	U32 moveCost;
};
Json_Begin(SpecialMoveCostDef)
	Json_Member("ability",  ability)
	Json_Member("moveCost", moveCost)
Json_End(SpecialMoveCostDef)

struct TerrainDef {
	Str                      name;
	Str                      sprite;
	ColorDef                 color;
	U32                      moveCost;
	Span<SpecialMoveCostDef> specialMoveCosts;
};
Json_Begin(TerrainDef)
	Json_Member("name",             name)
	Json_Member("sprite",           sprite)
	Json_Member("color",            color)
	Json_Member("moveCost",         moveCost)
	Json_Member("specialMoveCosts", specialMoveCosts)
Json_End(TerrainDef)

struct BattleMapDef {
	Span<Str>        spriteAtlasDefPaths;
	Str              highlightedHexSprite;
	ColorDef         highlightedHexColor;
	Str              selectedHexSprite;
	ColorDef         selectedHexColor;
	Span<TerrainDef> terrainDefs;
};
Json_Begin(BattleMapDef)
	Json_Member("spriteAtlasDefPaths",  spriteAtlasDefPaths)
	Json_Member("highlightedHexSprite", highlightedHexSprite)
	Json_Member("highlightedHexColor",  highlightedHexColor)
	Json_Member("selectedHexSprite",    selectedHexSprite)
	Json_Member("selectedHexColor",     selectedHexColor)
	Json_Member("terrainDefs",          terrainDefs)
Json_End(BattleMapDef)

//--------------------------------------------------------------------------------------------------

Res<> LoadBattleMapDef(Str battleMapDefPath) {
	Span<char> json; TryTo(File::ReadAllZ(battleMapDefPath), json);
	BattleMapDef battleMapDef; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &battleMapDef));

	for (U64 i = 0; i < battleMapDef.spriteAtlasDefPaths.len; i++) {
		Try(Draw::LoadSpriteAtlasDef(battleMapDef.spriteAtlasDefPaths[i]));
	}
	battleMapDef.highlightedHexSprite;
	battleMapDef.highlightedHexColor;
	battleMapDef.selectedHexSprite;
	battleMapDef.selectedHexColor;
	battleMapDef.terrainDefs;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Terrain GetTerrain(Str name) {
}

//--------------------------------------------------------------------------------------------------

Hex CreateHex(HexDef hexDef) {
}

//--------------------------------------------------------------------------------------------------

void DestroyHex(Hex hex) {
}

//--------------------------------------------------------------------------------------------------

void HighlightHex(Hex hex) {
}

//--------------------------------------------------------------------------------------------------

void SelectHex(Hex hex) {
}

//--------------------------------------------------------------------------------------------------

static Vec2 ColRowToTopLeftScreenPos(U32 col, U32 row) {
	return {
		(F32)((HexSize / 2) + col * HexSize + (row & 1) * (HexSize / 2)),
		(F32)((HexSize / 2) + row * (HexSize * 3 / 4)),
	};
}

//---------------------------------------------------------------------------------------------

void Draw(F32 drawZ) {
	for (U32 i = 0; i < hexObjsLen; i++) {
		HexObj const* const hexObj = &hexObjs[i];
		Draw::DrawSprite({
			.sprite = hexObj->terrainObj->sprite,
			.pos    = ColRowToTopLeftScreenPos(hexObj->pos.col, hexObj->pos.row),
			.z      = drawZ,
			.origin = Draw::Origin::TopLeft,
			.color  = hexObj->terrainObj->color,
		});
	}
	if (highlightedHexObj) {
		Draw::DrawSprite({
			.sprite = highlightedHexSprite,
			.pos    = ColRowToTopLeftScreenPos(highlightedHexObj->pos.col, highlightedHexObj->pos.row),
			.z      = drawZ,
			.origin = Draw::Origin::TopLeft,
			.color  = highlightedHexColor,
		});
	}
	if (selectedHexObj) {
		Draw::DrawSprite({
			.sprite = selectedHexSprite,
			.pos    = ColRowToTopLeftScreenPos(selectedHexObj->pos.col, selectedHexObj->pos.row),
			.z      = drawZ,
			.origin = Draw::Origin::TopLeft,
			.color  = selectedHexColor,
		});
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::BMap