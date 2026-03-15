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

namespace NeighborIdx {
	constexpr U8 TopLeft     = 0;
	constexpr U8 TopRight    = 1;
	constexpr U8 Right       = 2;
	constexpr U8 BottomRight = 3;
	constexpr U8 BottomLeft  = 4;
	constexpr U8 Left        = 5;
	constexpr U8 Max         = 6;
}

struct Hex {
	U16            idx;
	U16            c, r;
	Vec2           pos;
	Hex*           neighbors[6];	// indexed by NeighborIdx::*; nullptr = no neighbor
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

enum DrawType : U8 {
	DrawType_None = 0,
	DrawType_FriendlyMoveable,
	DrawType_FriendlyAttackable,
	DrawType_EnemyAttackable,
	DrawType_FriendlyMoveableAndEnemyAttackable,
	DrawType_EnemyAttacker,
	DrawType_HoverDefault,
	DrawType_HoverMoveable,
	DrawType_HoverSelectableFriendly,
	DrawType_HoverAttackableEnemy,
	DrawType_HoverUnattackableEnemy,
	DrawType_Selected,
	DrawType_HoverPathTopLeft,
	DrawType_HoverPathTopRight,
	DrawType_HoverPathRight,
	DrawType_HoverPathBottomRight,
	DrawType_HoverPathBottomLeft,
	DrawType_HoverPathLeft,
	DrawType_HoverAttack,
	DrawType_TargetPathTopLeft,
	DrawType_TargetPathTopRight,
	DrawType_TargetPathRight,
	DrawType_TargetPathBottomRight,
	DrawType_TargetPathBottomLeft,
	DrawType_TargetPathLeft,
	DrawType_TargetAttack,
	DrawType_Max,
};

struct DrawObj {
	Vec2     pos;
	DrawType type;
};

constexpr U16 MaxDrawPath = 2 * MaxHexes + 1; // two draw objs for each hex + attack

struct DrawDef {
	DrawObj overlay[MaxHexes];
	U16     overlayLen;
	DrawObj hoverSelected[2];
	DrawObj hoverPath[MaxDrawPath];
	U16     hoverPathLen;
	DrawObj targetPath[MaxDrawPath];
	U16     targetPathLen;
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
void             Draw(Data const* data, DrawDef const* drawDef);
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