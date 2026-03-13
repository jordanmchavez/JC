#pragma once

#include "JC/Common.h"

namespace JC::Draw    { DefHandle(Sprite); }
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
	Str               highlightSprite;
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

namespace NeighborIdx {
	constexpr U8 TopLeft     = 0;
	constexpr U8 TopRight    = 1;
	constexpr U8 Right       = 2;
	constexpr U8 BottomRight = 3;
	constexpr U8 BottomLeft  = 4;
	constexpr U8 Left        = 5;
}

namespace HexFlags {
	constexpr U64 FriendlyMoveableOrAttackable = (U64)1 << 0;
	constexpr U64 EnemyAttackable              = (U64)1 << 2;
	constexpr U64 EnemyAttacker                = (U64)1 << 3;
	constexpr U64 PathTopLeft                  = (U64)1 << 4;
	constexpr U64 PathTopRight                 = (U64)1 << 5;
	constexpr U64 PathRight                    = (U64)1 << 6;
	constexpr U64 PathBottomRight              = (U64)1 << 7;
	constexpr U64 PathBottomLeft               = (U64)1 << 8;
	constexpr U64 PathLeft                     = (U64)1 << 9;
	constexpr U64 AttackTopLeft                = (U64)1 << 10;
	constexpr U64 AttackTopRight               = (U64)1 << 11;
	constexpr U64 AttackRight                  = (U64)1 << 12;
	constexpr U64 AttackBottomRight            = (U64)1 << 13;
	constexpr U64 AttackBottomLeft             = (U64)1 << 14;
	constexpr U64 AttackLeft                   = (U64)1 << 15;
	constexpr U64 HoverValid                   = (U64)1 << 16;
	constexpr U64 HoverInvalid                 = (U64)1 << 20;
	constexpr U64 Selected                     = (U64)1 << 17;
	constexpr U64 SelectedAttackTarget         = (U64)1 << 19;
};

/*
Reachable by Friendly
Targettable by Enemy
Both
Enemies who can Target
Path
Selected
Selected Attack Target
Actionable Hover
Unactionable Hover*/
// TODO: split out into SoA as needed
struct Hex {
	U16            idx;
	U16            c, r;
	Vec2           pos;
	Hex*           neighbors[6];	// indexed by NeighborIdx_*; nullptr = no neighbor
	Terrain const* terrain;
	struct Unit*   unit;
	U64            flags;
};

struct Side {
	enum Val : U8 { Left = 0, Right, Max };
	Val val;
	Side() { val = Side::Left; }
	Side(Val valIn) { val = valIn; }
	operator U8() const { return val; }
	Side& operator++() { val = (Val)(val + 1); return *this; }
	Side operator++(int) { Side tmp = *this; val = (Val)(val + 1); return tmp; }
};

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

enum struct State {
	WaitingOrder,
	ExecutingOrder,
};

// for each hex: list of enemies who can move attack that hex

constexpr U32 MaxClickHexes = 16;

struct Data {
	Hex   hexes[MaxHexes];
	U32   hexesLen;
	Vec2  cameraPos;
	F32   cameraScale;
	Army  armies[2];
	U8    activeSide;
	Hex*  hoverHex;
	Hex*  selectedHex;
	Hex*  targetHex;
	Path  selectedPath;
	bool  showEnemyThreatMap;
	State state;
};

//--------------------------------------------------------------------------------------------------

// Battle_Draw.cpp
Res<> InitDraw(Data* data, Mem tempMem, Window::State const* windowState);
Hex * ScreenPosToHex(Data* data, I32 x, I32 y);
void  MoveCamera(Data* data, F32 sec, F32 dx, F32 dy);
void  ZoomCamera(Data* data, F32 d);
void  Draw(Data const* data);
Res<> LoadDraw(BattleJson const* battleJson);

// Battle_Input.cpp
void  InitInput(Mem tempMem);
Res<> HandleInput(Data* data, F32 sec, U32 mouseX, U32 mouseY, Span<U64 const> actionIds);

// Battle_Path.cpp
void  InitPath(Mem permMem);
void  BuildPathMap(Hex* hexes, Unit* unit);
bool  FindPath(Unit const* unit, Hex const* end, Path* pathOut);

// Battle_Util.cpp
void  InitUtil();
bool  AreHexesAdjacent(Hex const* a, Hex const* b);
Vec2  ColRowToTopLeftWorldPos(I32 c, I32 r);
U32   HexDistance(Hex const* a, Hex const* b);
Hex*  WorldPosToHex(Data* data, Vec2 p);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle