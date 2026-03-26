#include "JC/Battle_Map.h"

#include "JC/Draw.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"
#include "JC/Rng.h"

namespace JC::Battle::Map {

//--------------------------------------------------------------------------------------------------

DefErr(Battle_Map, MaxTerrain);

//--------------------------------------------------------------------------------------------------

struct TerrainDef {
	Str name;
	Str sprite;
	U32 chance;
	U32 movementCost;
	U32 staminaCost;
};
Json_Begin(TerrainDef)
	Json_Member   ("name",         name)
	Json_Member   ("sprite",       sprite)
	Json_Member   ("chance",       chance)
	Json_Member   ("movementCost", movementCost)
	Json_MemberOpt("staminaCost",  staminaCost)
Json_End(TerrainDef)

struct MapDef {
	Span<TerrainDef> terrain;
	Str              font;
};
Json_Begin(MapDef)
	Json_Member("terrain", terrain)
	Json_Member("font",    font)
Json_End(MapDef)

//--------------------------------------------------------------------------------------------------

static constexpr U32 HexSize     = 32;
static constexpr U32 MaxTerrains = 128;
static constexpr U32 MaxHexes    = 16 * 16;

namespace Dir {
	constexpr U32 TopLeft     = 0;
	constexpr U32 TopRight    = 1;
	constexpr U32 Right       = 2;
	constexpr U32 BottomRight = 3;
	constexpr U32 BottomLeft  = 4;
	constexpr U32 Left        = 5;
	constexpr U32 Max         = 6;
}

struct Terrain {
	Str          name;
	Draw::Sprite sprite;
	U32          movementCost;
	U32          staminaCost;
};

struct TerrainChance {
	Terrain const* terrain;
	U32            chance;
};

struct Hex {
	U32            c, r;
	Vec2           pos;
	Terrain const* terrain;
	Hex*           neighbors[6];
};

static Mem                         tempMem;
static U32                         hexSize;
static Array<Terrain>              terrains;
static JC::Map<Str, Terrain*>      terrainsMap;
static Array<TerrainChance>        terrainChances;
static Array<Hex>                  hexes;
static Draw::Font                  font;
static Array<Draw::DrawSpriteDesc> hexDrawDescs;
static F32                         drawZ;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMemIn, F32 drawZIn) {
	tempMem = tempMemIn;
	terrains.Init(permMem, MaxTerrains);
	terrainsMap.Init(permMem, MaxTerrains);
	terrainChances.Init(permMem, MaxTerrains);
	hexes.Init(permMem, MaxHexes);
	hexDrawDescs.Init(permMem, MaxHexes);
	drawZ = drawZIn;
}

//--------------------------------------------------------------------------------------------------

static void RebuildHexDrawDescs() {
	hexDrawDescs.len = 0;
	for (U64 i = 0; i < hexes.len; i++) {
		hexDrawDescs[i] = {
			.sprite = hexes[i].terrain->sprite,
			.pos    = hexes[i].pos,
			.z      = drawZ,
		};
	}
}

//--------------------------------------------------------------------------------------------------

Res<> Load(Str path) {
	MapDef mapDef; Try(Json::Load(tempMem, path, &mapDef));

	if (!terrains.HasCapacity(mapDef.terrain.len)) { return Err_MaxTerrain(); }

	TryTo(Draw::GetFont(mapDef.font), font);

	U32 accumulatedChance = terrainChances.len > 0 ? terrainChances[terrainChances.len - 1].chance : 0;
	for (U64 i = 0; i < mapDef.terrain.len; i++) {
		TerrainDef const* terrainDef = &mapDef.terrain[i];
		Draw::Sprite sprite; TryTo(Draw::GetSprite(terrainDef->sprite), sprite);
		Terrain* terrain = terrains.Add();
		*terrain = {
			.name         = terrainDef->name,	// interned by Json
			.sprite       = sprite,
			.movementCost = terrainDef->movementCost,
			.staminaCost  = terrainDef->staminaCost,
		};
		terrainsMap.Put(terrainDef->name, terrain);
		accumulatedChance += terrainDef->chance;
		terrainChances.Add({
			.terrain = terrain,
			.chance  = accumulatedChance,
		});
	}

	RebuildHexDrawDescs();

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Vec2 CalcWorldPos(U32 c, U32 r) {
	return {
		(F32)((HexSize / 2) + (c * HexSize + (r & 1) * (HexSize / 2))),
		(F32)((HexSize / 2) + (r * (HexSize * 3 / 4))),
	};
}

//--------------------------------------------------------------------------------------------------

static Terrain const* RngTerrain() {
	Assert(terrainChances.len > 0);
	U32 roll = Rng::NextU32(0, terrainChances[terrainChances.len - 1].chance);
	for (U64 i = 0; i < terrainChances.len; i++) {
		if (roll < terrainChances[i].chance) {
			return terrainChances[i].terrain;
		}
	}
	Panic("roll %u was higher than max terrain chance %u???", roll, terrainChances[terrainChances.len - 1].chance);
}

//--------------------------------------------------------------------------------------------------

void GenerateRandomMap(U32 cols, U32 rows) {
	Assert(cols * rows <= MaxHexes);
	JC::Map<U64, Hex*> colRowMap(tempMem, cols * rows);
	auto Key = [](U32 c, U32 r) { return (U64)c | ((U64)r << 32); };

	hexes.len = 0;

	for (U32 c = 0; c < cols; c++) {
		for (U32 r = 0; r < rows; r++) {
			Hex* hex     = hexes.Add();
			hex->c       = c;
			hex->r       = r;
			hex->pos     = CalcWorldPos(c, r);
			hex->terrain = RngTerrain();
			colRowMap.Put(Key(c, r), hex);
		}
	}

	for (U32 c = 0; c < cols; c++) {
		for (U32 r = 0; r < rows; r++) {
			Hex* hex = colRowMap.FindOrZero(Key(c, r));
			if (!hex) { return; }
			if (r & 1) {	// odd row
				hex->neighbors[Dir::TopLeft    ] = colRowMap.FindOrZero(Key(c    , r - 1));
				hex->neighbors[Dir::TopRight   ] = colRowMap.FindOrZero(Key(c + 1, r - 1));
				hex->neighbors[Dir::Right      ] = colRowMap.FindOrZero(Key(c + 1, r    ));
				hex->neighbors[Dir::BottomRight] = colRowMap.FindOrZero(Key(c + 1, r + 1));
				hex->neighbors[Dir::BottomLeft ] = colRowMap.FindOrZero(Key(c    , r + 1));
				hex->neighbors[Dir::Left       ] = colRowMap.FindOrZero(Key(c - 1, r    ));
			} else {	// even row
				hex->neighbors[Dir::TopLeft    ] = colRowMap.FindOrZero(Key(c - 1, r - 1));
				hex->neighbors[Dir::TopRight   ] = colRowMap.FindOrZero(Key(c    , r - 1));
				hex->neighbors[Dir::Right      ] = colRowMap.FindOrZero(Key(c + 1, r    ));
				hex->neighbors[Dir::BottomRight] = colRowMap.FindOrZero(Key(c    , r + 1));
				hex->neighbors[Dir::BottomLeft ] = colRowMap.FindOrZero(Key(c - 1, r + 1));
				hex->neighbors[Dir::Left       ] = colRowMap.FindOrZero(Key(c - 1, r    ));
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

void Draw() {
	Draw::DrawSprites(hexDrawDescs);
	/*
	Draw::DrawStr({
		.font   = font,
		.str    = SPrintf(tempMem, "%u,%u[%u]", hex->c, hex->r, hex->idx),
		.pos    = Vec2(hex->pos.x, hex->pos.y + HexSize / 2 - 2.f),
		.z      = Z_Ui + 10,
		.origin = Draw::Origin::BottomCenter,
		.scale = Vec2(1.f / cameraScale, 1.f / cameraScale),
		.color  = Vec4(1.f, 1.f, 1.f, 1.f),
	});
	*/
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle::Map