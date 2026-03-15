#pragma once

#include "JC/Common.h"

namespace JC::Draw    { DefHandle(Sprite); }
namespace JC::Input   { struct Action; }
namespace JC::UnitDef { struct Def; }
namespace JC::Window  { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

constexpr U8  HexSize      = 32;
constexpr U16 MaxCols      = 16;
constexpr U16 MaxRows      = 16;
constexpr U16 MaxHexes     = MaxCols * MaxRows;
constexpr U16 MaxArmyUnits = 64;

struct TerrainJson {
	Str name;
	Str sprite;
	U32 moveCost;
};

struct BattleJson {
	Span<Str>         atlasPaths;
	Str               borderSprite;
	Str               innerSprite;
	Str               pathTopLeftSprite;
	Str               pathTopRightSprite;
	Str               pathRightSprite;
	Str               pathBottomRightSprite;
	Str               pathBottomLeftSprite;
	Str               pathLeftSprite;
	Str               arrowTopLeftSprite;
	Str               arrowTopRightSprite;
	Str               arrowRightSprite;
	Str               arrowBottomRightSprite;
	Str               arrowBottomLeftSprite;
	Str               arrowLeftSprite;
	Span<TerrainJson> terrain;
};

struct Terrain {
	Str          name;
	Draw::Sprite sprite;
	U16          moveCost;
};

enum NeighborIdx : U8 {
	NeighborIdx_TopLeft     = 0,
	NeighborIdx_TopRight    = 1,
	NeighborIdx_Right       = 2,
	NeighborIdx_BottomRight = 3,
	NeighborIdx_BottomLeft  = 4,
	NeighborIdx_Left        = 5,
};

struct Hex {
	U16            idx;
	U16            c, r;
	Vec2           pos;
	Hex*           neighbors[6];	// indexed by NeighborIdx_*; nullptr = no neighbor
	Terrain const* terrain;
	struct Unit*   unit;
};

enum Side : U8 { Side_Left = 0, Side_Right, Side_Max };
inline Side operator++(Side& side, int) { Side tmp = side; side = (Side)((U8)side + 1); return tmp; }

struct Path {
	Hex* hexes[MaxHexes];
	U16  len;
};

struct PathMap {
	U16  moveCosts[MaxHexes];
	Hex* parents[MaxHexes];
};

struct Unit {
	UnitDef::Def const* def;
	Hex*                hex;
	Vec2                pos;
	Side                side;
	U32                 hp;
	U32                 move;
	U32                 range;
	PathMap             pathMap;
};

struct Army {
	Side side;
	Unit units[MaxArmyUnits];
	U32  unitsLen;
	U64  attackMap[MaxHexes];	// [c, r] = bitmap of units that can attack this spot, updated on any unit create/destroy/move
};

enum struct DrawHover : U8 {
	None = 0,
	FriendlyMoveable,
	FriendlySelectable,
	FriendlyAttackable,
	Enemy,
};

namespace OverlayFlags {
	constexpr U64 FriendlyThreat = (U64)1 << 0;
	constexpr U64 EnemyThreat    = (U64)1 << 1;
	constexpr U64 Attacker       = (U64)1 << 2;
};

struct DrawDef {
	DrawHover        hover;
	U64              overlayFlags[MaxHexes];
	Span<Vec2 const> hoverPath[6];
	Span<Vec2 const> targetPath[6];
};

struct Data {
	Hex  hexes[MaxHexes];
	U32  hexesLen;
	Army armies[2];
	U8   activeSide;
};

struct InputResult {
	Span<Hex const*> clickHexes;	// nullptr = right click
	Hex const*       hoverHex;
	bool             showEnemyArmyThreatMap;
};

//--------------------------------------------------------------------------------------------------

// Battle_Draw.cpp
Res<>            InitDraw(Mem tempMem, Window::State const* windowState);
Hex *            ScreenPosToHex(Data* data, I32 x, I32 y);
void             MoveCamera(F32 sec, F32 dx, F32 dy);
void             ZoomCamera(F32 d);
void             Draw(Data* data, DrawDef const* drawDef);
Res<>            LoadDraw(BattleJson const* battleJson);

// Battle_Input.cpp
void             InitInput(Mem permMem, Mem tempMem);
Res<InputResult> HandleInput(Data* data, F32 sec, U32 mouseX, U32 mouseY, Span<Input::Action const> actionIds);

// Battle_Path.cpp
void             InitPath(Mem permMem);
void             BuildPathMap(Hex* hexes, Unit* unit);
bool             FindPath(Unit const* unit, Hex const* end, Path* pathOut);

// Battle_Util.cpp
void             InitUtil();
bool             AreHexesAdjacent(Hex const* a, Hex const* b);
Vec2             ColRowToTopLeftWorldPos(I32 c, I32 r);
U32              HexDistance(Hex const* a, Hex const* b);
Hex*             WorldPosToHex(Data* data, Vec2 p);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle