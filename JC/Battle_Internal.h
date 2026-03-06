#pragma once

#include "JC/Common.h"

namespace JC::Draw   { DefHandle(Sprite); }
namespace JC::Unit   { struct Data; }
namespace JC::Window { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

constexpr U32 HexSize      = 32;
constexpr I32 MaxCols      = 16;
constexpr I32 MaxRows      = 14;
constexpr U32 MaxArmyUnits = 16;

struct TerrainJson {
	Str name;
	Str sprite;
	U32 moveCost;
};

struct BattleJson {
	Span<Str>         atlasPaths;
	Str               borderSprite;
	Str               highlightSprite;
	Str               pathTopLeftSprite;
	Str               pathTopRightSprite;
	Str               pathRightSprite;
	Str               pathBottomRightSprite;
	Str               pathBottomLeftSprite;
	Str               pathLeftSprite;
	Span<TerrainJson> terrain;
};

struct Terrain {
	Str          name;
	Draw::Sprite sprite;
	U32          moveCost;
};


constexpr U32 NeighborIdx_TopLeft     = 0;
constexpr U32 NeighborIdx_TopRight    = 1;
constexpr U32 NeighborIdx_Right       = 2;
constexpr U32 NeighborIdx_BottomRight = 3;
constexpr U32 NeighborIdx_BottomLeft  = 4;
constexpr U32 NeighborIdx_Left        = 5;

struct Hex {
	U32            idx;
	I32            c, r;
	Hex*           neighbors[6];	// indexed by NeighborIdx_*; nullptr = no neighbor
	Terrain const* terrain;
	UnitData*      unit;
};

struct PathMap {
	U32        moveCosts[MaxCols * MaxRows];
	U32        pathLens[MaxCols * MaxRows];
	Hex const* parents[MaxCols * MaxRows];
};

struct Path {
	Hex const* hexes[MaxCols * MaxRows];
	U32        len;
};

struct UnitData {
	Unit::DefData const* def;
	Unit::Data*          ??;
	Hex const*           hex;
	Vec2                 pos;
	U32                  move;
};

struct Army {
	U32       side;
	UnitData* units[MaxArmyUnits];
	U32       unitsLen;
};

enum struct State {
	WaitingOrder,
	UnitSelected,
	ExecutingOrder,
};

struct Data {
	Hex*       hexes;
	U32        hexesLen;
	Vec2       cameraPos;
	F32        cameraScale;
	Army       armies[2];
	Hex const* hoverHex;
	Hex const* selectedHex;
	PathMap    selectedHexPathMap;
	bool       selectedHexAttackable[MaxRows * MaxCols];
	Path       selectedHexToHoverHexPath;
	State      state;
};

//--------------------------------------------------------------------------------------------------

// Battle_Draw.cpp
Res<>      InitDraw(Data* data, Mem tempMem, Window::State const* windowState);
Hex const* ScreenPosToHex(Data const* data, I32 x, I32 y);
void       MoveCamera(Data* data, F32 sec, F32 dx, F32 dy);
void       ZoomCamera(Data* data, F32 d);
void       Draw(Data const* data);
Res<>      LoadDraw(BattleJson const* battleJson);

// Battle_Input.cpp
void       InitInput();
Res<>      HandleActions(Data* data, F32 sec, Span<U64 const> actionIds);

// Battle_Path.cpp
void       InitPath(Mem permMem);
void       BuildPathMap(Hex const* hexes, Hex const* startHex, U32 move, U32 side, PathMap* pathMapOut);
void       BuildAttackableMap(Hex const* hexes, PathMap const* pathMap, Army const* enemyArmy, U32 range, bool* attackableMapOut);
bool       FindPathFromMoveCostMap(PathMap const* pathMap, Hex const* startHex, Hex const* end, Path* pathOut);

// Battle_Util.cpp
void       InitUtil();
Vec2       ColRowToTopLeftWorldPos(I32 c, I32 r);
U32        HexDistance(Hex const* a, Hex const* b);
Vec2       HexToTopLeftWorldPos(Hex const* hex);
Vec2       HexToCenterWorldPos(Hex const* hex);
Hex const* WorldPosToHex(Data const* data, Vec2 p);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle