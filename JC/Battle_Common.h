#pragma once

#include "JC/Common.h"

namespace JC::Draw    { DefHandle(Sprite); }
namespace JC::Input   { struct Action; }
namespace JC::Window  { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

constexpr U8  HexSize      = 32;
constexpr U16 MaxCols      = 16;
constexpr U16 MaxRows      = 16;
constexpr U16 MaxHexes     = MaxCols * MaxRows;
constexpr U16 MaxArmyUnits = 64;

struct TerrainDef {
	Str name;
	Str sprite;
	U32 moveCost;
};

struct BattleDef {
	Span<Str>        atlasPaths;
	Str              numberFont;
	Str              uiFont;
	Str              fancyFont;
	Str              borderSprite;
	Str              innerSprite;
	Str              pathTopLeftSprite;
	Str              pathTopRightSprite;
	Str              pathRightSprite;
	Str              pathBottomRightSprite;
	Str              pathBottomLeftSprite;
	Str              pathLeftSprite;
	Str              arrowTopLeftSprite;
	Str              arrowTopRightSprite;
	Str              arrowRightSprite;
	Str              arrowBottomRightSprite;
	Str              arrowBottomLeftSprite;
	Str              arrowLeftSprite;
	Span<TerrainDef> terrain;
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
	Vec2                pos;
	PathMap             pathMap;
};

struct Army {
	Unit units[MaxArmyUnits];
	U8   unitsLen;
	U64  attackMap[MaxHexes];	// [c, r] = bitmap of units that can attack this spot, updated on any unit create/destroy/move
};

enum DrawType : U8 {
	DrawType_None = 0,
	DrawType_FriendlyMoveable,
	DrawType_FriendlyAttackable,
	DrawType_EnemyAttackable,
	DrawType_FriendlyMoveableAndEnemyAttackable,
	DrawType_EnemyAttacker,
	DrawType_HoverInteractible,
	DrawType_HoverNoninteractible,
	DrawType_Selected,
	// Must match Dir order
	DrawType_HoverPathBase,
	DrawType_HoverPathTopLeft = DrawType_HoverPathBase,
	DrawType_HoverPathTopRight,
	DrawType_HoverPathRight,
	DrawType_HoverPathBottomRight,
	DrawType_HoverPathBottomLeft,
	DrawType_HoverPathLeft,
	DrawType_HoverAttack,
	// Must match Dir order
	DrawType_TargetPathBase,
	DrawType_TargetPathTopLeft = DrawType_TargetPathBase,
	DrawType_TargetPathTopRight,
	DrawType_TargetPathRight,
	DrawType_TargetPathBottomRight,
	DrawType_TargetPathBottomLeft,
	DrawType_TargetPathLeft,
	DrawType_TargetAttack,
	DrawType_Max,
};

static_assert(DrawType_HoverPathTopLeft      == DrawType_HoverPathBase + Dir::TopLeft);
static_assert(DrawType_HoverPathTopRight     == DrawType_HoverPathBase + Dir::TopRight);
static_assert(DrawType_HoverPathRight        == DrawType_HoverPathBase + Dir::Right);
static_assert(DrawType_HoverPathBottomRight  == DrawType_HoverPathBase + Dir::BottomRight);
static_assert(DrawType_HoverPathBottomLeft   == DrawType_HoverPathBase + Dir::BottomLeft);
static_assert(DrawType_HoverPathLeft         == DrawType_HoverPathBase + Dir::Left);

static_assert(DrawType_TargetPathTopLeft     == DrawType_TargetPathBase+ Dir::TopLeft);
static_assert(DrawType_TargetPathTopRight    == DrawType_TargetPathBase+ Dir::TopRight);
static_assert(DrawType_TargetPathRight       == DrawType_TargetPathBase+ Dir::Right);
static_assert(DrawType_TargetPathBottomRight == DrawType_TargetPathBase+ Dir::BottomRight);
static_assert(DrawType_TargetPathBottomLeft  == DrawType_TargetPathBase+ Dir::BottomLeft);
static_assert(DrawType_TargetPathLeft        == DrawType_TargetPathBase+ Dir::Left);

struct DrawObj {
	Vec2     pos;
	DrawType type;
};

constexpr U16 MaxDrawPath = 2 * MaxHexes + 1; // two draw objs for each hex + attack

struct DrawDef {
	DrawObj overlay[MaxHexes];
	U16     overlayLen;
	DrawObj hover;
	DrawObj selected;
	DrawObj path[MaxDrawPath];	// includes the attack drawObj, if any
	U16     pathLen;
};

struct Shared {
	Hex  hexes[MaxHexes];
	U32  hexesLen;
	Army armies[2];
	U8   activeSide;
};

//--------------------------------------------------------------------------------------------------

// Battle.cpp
void             SelectNextUnit();
void             EndSelectedUnitTurn();

// Battle_Draw.cpp
void             InitDraw(Mem tempMem, Shared* shared, Window::State const* windowState);
Hex *            ScreenPosToHex(I32 x, I32 y);
void             MoveCamera(F32 sec, F32 dx, F32 dy);
void             ZoomCamera(F32 d);
void             Draw(DrawDef const* drawDef);
Res<>            LoadDraw(BattleDef const* battleJson);

// Battle_Input.cpp
void             InitInput(Mem permMem, Mem tempMem);
Res<InputResult> HandleInput(F32 sec, U32 mouseX, U32 mouseY, Span<Input::Action const> actionIds);

// Battle_Path.cpp
void             InitPath(Mem permMem);
void             BuildPathMap(Hex* hexes, Unit* unit);
void             BuildPathOrPanic(Unit const* unit, Hex* end, Path* pathOut);

// Battle_Util.cpp
void             InitUtil(Shared* shared);
bool             AreHexesAdjacent(Hex const* a, Hex const* b);
Vec2             ColRowToTopLeftWorldPos(I32 c, I32 r);
U32              HexDistance(Hex const* a, Hex const* b);
Hex*             WorldPosToHex(Vec2 p);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle