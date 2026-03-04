#include "JC/BattleMap.h"

#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Log.h"
#include "JC/Map.h"

namespace JC::BMap {

//--------------------------------------------------------------------------------------------------

DefErr(BMap, UnknownTerrain);

static constexpr U32 HexSize     = 32;
static constexpr U32 MaxTerrains = 128;
static constexpr U32 MaxHexes    = 4096;	// 64x64

struct TerrainObj {
	Str          name;
	Draw::Sprite sprite;
	U32          moveCost;
};

struct HexObj {
	TerrainObj const* terrainObj;
	HexPos            pos;
	bool              border;
	Vec4              borderColor;
	bool              highlight;
	Vec4              highlightColor;
};

static Mem           tempMem;
static U8            hexLut[(HexSize / 2) * (HexSize * 3 / 8)];
static TerrainObj*   terrainObjs;
static U32           terrainObjsLen;
static Map<Str, U32> terrainMap;
static HexObj*       hexObjs;
static U32           hexObjsLen;
static Draw::Sprite  borderSprite;
static Draw::Sprite  highlightSprite;

//--------------------------------------------------------------------------------------------------
/*
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
/*/

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
	terrainMap.Init(permMem, MaxTerrains);
	hexObjs        = Mem::AllocT<HexObj>    (permMem, MaxHexes);
	hexObjsLen     = 1;	// reserve 0 for invalid
	InitLut();
}

//--------------------------------------------------------------------------------------------------

struct TerrainJson {
	Str name;
	Str sprite;
	U32 moveCost;
};
Json_Begin(TerrainJson)
	Json_Member("name",     name)
	Json_Member("sprite",   sprite)
	Json_Member("moveCost", moveCost)
Json_End(TerrainJson)

struct BattleMapJson {
	Span<Str>         spriteAtlasJsonPaths;
	Str               borderSprite;
	Str               highlightSprite;
	Span<TerrainJson> terrain;
};
Json_Begin(BattleMapJson)
	Json_Member("spriteAtlasJsonPaths", spriteAtlasJsonPaths)
	Json_Member("borderSprite",         borderSprite)
	Json_Member("highlightSprite",      highlightSprite)
	Json_Member("terrain",              terrain)
Json_End(BattleMapJson)

//--------------------------------------------------------------------------------------------------

Res<> LoadBattleMapJson(Str battleMapJsonPath) {
	Span<char> json; TryTo(File::ReadAllZ(battleMapJsonPath), json);
	BattleMapJson battleMapJson; Try(Json::ToObject(tempMem, tempMem, json.data, (U32)json.len, &battleMapJson));

	for (U64 i = 0; i < battleMapJson.spriteAtlasJsonPaths.len; i++) {
		Try(Draw::LoadSpriteAtlasJson(battleMapJson.spriteAtlasJsonPaths[i]));
	}

	TryTo(Draw::GetSprite(battleMapJson.borderSprite),    borderSprite);
	TryTo(Draw::GetSprite(battleMapJson.highlightSprite), highlightSprite);

	Assert(battleMapJson.terrain.len < MaxTerrains);
	terrainObjsLen = 1;	// reserve 0 for invalid
	for (U64 i = 0; i < battleMapJson.terrain.len; i++) {
		TerrainJson const* const terrainJson = &battleMapJson.terrain[i];
		Draw::Sprite sprite; TryTo(Draw::GetSprite(terrainJson->sprite), sprite);
		terrainObjs[terrainObjsLen] = {
			.name     = terrainJson->name,	// interned by Json
			.sprite   = sprite,
			.moveCost = terrainJson->moveCost,
		};
		terrainMap.Put(terrainJson->name, terrainObjsLen);
		terrainObjsLen++;
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Terrain> GetTerrain(Str name) {
	U32 const* const idxPtr = terrainMap.FindOrNull(name);
	if (!idxPtr) {
		return Err_UnknownTerrain("name", name);
	}

	return Terrain { .handle = 	(U64)(*idxPtr) };
}

//--------------------------------------------------------------------------------------------------

Hex CreateHex(Terrain terrain, HexPos pos) {
	Assert(hexObjsLen < MaxHexes);
	Assert(terrain.handle > 0 && terrain.handle < MaxTerrains);
	HexObj* const hexObj = &hexObjs[hexObjsLen++];
	memset(hexObj, 0, sizeof(HexObj));
	hexObj->terrainObj = &terrainObjs[terrain.handle];
	hexObj->pos        = pos;
	Logf("Created terrain '%s' at %ix%i", hexObj->terrainObj->name, pos.c, pos.r);
	return { .handle = (U64)(hexObjsLen - 1) };
}

//--------------------------------------------------------------------------------------------------

// TODO: consider using a handle pool here since we fail to handle use-after-destroy
void DestroyHex(Hex hex) {
	Assert(hex.handle > 0 && hex.handle < hexObjsLen);
	hexObjs[hex.handle] = hexObjs[hexObjsLen - 1];
	hexObjsLen--;
}

//--------------------------------------------------------------------------------------------------

void ShowHexBorder(Hex hex, Vec4 color) {
	Assert(hex.handle > 0 && hex.handle < hexObjsLen);
	HexObj* const hexObj = &hexObjs[hex.handle];
	hexObj->border = true;
	hexObj->borderColor = color;
}

//--------------------------------------------------------------------------------------------------

void HideHexBorder(Hex hex) {
	Assert(hex.handle > 0 && hex.handle < hexObjsLen);
	HexObj* const hexObj = &hexObjs[hex.handle];
	hexObj->border = false;
	hexObj->borderColor = Vec4();
}

//--------------------------------------------------------------------------------------------------

void ShowHexHighlight(Hex hex, Vec4 color) {
	Assert(hex.handle > 0 && hex.handle < hexObjsLen);
	HexObj* const hexObj = &hexObjs[hex.handle];
	hexObj->highlight = true;
	hexObj->highlightColor = color;
}

//--------------------------------------------------------------------------------------------------

void HideHexHighlight(Hex hex) {
	Assert(hex.handle > 0 && hex.handle < hexObjsLen);
	HexObj* const hexObj = &hexObjs[hex.handle];
	hexObj->highlight = false;
	hexObj->highlightColor = Vec4();
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
	for (U32 i = 1; i < hexObjsLen; i++) {
		HexObj const* const hexObj = &hexObjs[i];
		Draw::SpriteDrawDef drawDef = {
			.sprite = hexObj->terrainObj->sprite,
			.pos    = ColRowToTopLeftScreenPos(hexObj->pos.c, hexObj->pos.r),
			.z      = drawZ,
			.origin = Draw::Origin::TopLeft,
		};
		Draw::DrawSprite(drawDef);
		if (hexObj->border) {
			drawDef.sprite = borderSprite;
			drawDef.color  = hexObj->borderColor;
			Draw::DrawSprite(drawDef);
		}
		if (hexObj->highlight) {
			drawDef.sprite = highlightSprite;
			drawDef.color  = hexObj->highlightColor;
			Draw::DrawSprite(drawDef);
		}
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::BMap