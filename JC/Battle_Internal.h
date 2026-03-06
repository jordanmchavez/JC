#pragma once

#include "JC/Common.h"

namespace JC::Draw   { DefHandle(Sprite); }
namespace JC::Unit   { struct Data; enum struct Side; }
namespace JC::Window { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

constexpr U32 HexSize = 32;
constexpr I32 MaxCols = 16;
constexpr I32 MaxRows = 14;

struct Terrain {
	Str          name;
	Draw::Sprite sprite;
	U32          moveCost;
};

struct Hex {
	U32            idx;
	I32            c, r;
	Hex*           neighbors[6];
	U32            neighborsLen;
	Terrain const* terrain;
	Unit::Data*    unitData;
};

struct MoveCostMap {
	U32        moveCosts[MaxCols * MaxRows];
	Hex const* parents[MaxCols * MaxRows];
};

struct Path {
	Hex const* hexes[MaxCols * MaxRows];
	U32        len;
};

enum struct State {
	None,
	UnitSelected,
	ExecutingOrder,
};

struct Data {
	Hex*         hexes;
	U32          hexesLen;
	Draw::Sprite borderSprite;
	Draw::Sprite highlightSprite;
	Hex const*   hoverHex;
	Hex const*   selectedHex;
	MoveCostMap  selectedHexMoveCostMap;
	Path         selectedHexToHoverHexPath;
	State        state;
};

//--------------------------------------------------------------------------------------------------

// Battle_Draw.cpp
Res<>      InitDraw(Mem tempMem, Window::State const* windowState);

Hex const* ScreenPosToHex(Data const* data, I32 x, I32 y);
void       MoveCamera(F32 sec, F32 dx, F32 dy);
void       ZoomCamera(F32 d);
void       Draw(Data const* data);

// Battle_Input.cpp
void       InitInput();
Res<>      HandleActions(Data* data, F32 sec, Span<U64 const> actionIds);

// Battle_Path.cpp
void       InitPath(Mem permMem);
void       BuildMoveCostMap(Data const* data, Hex const* startHex, U32 move, Unit::Side side, MoveCostMap* moveCostMap);
bool       FindPathFromMoveCostMap(MoveCostMap const* moveCostMap, Hex const* startHex, Hex const* end, Path* pathOut);

// Battle_Util.cpp
void       InitUtil();
Vec2       ColRowToTopLeftWorldPos(I32 c, I32 r);
Vec2       HexToTopLeftWorldPos(Hex const* hex);
Vec2       HexToCenterWorldPos(Hex const* hex);
Hex const* WorldPosToHex(Data const* data, Vec2 p);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle