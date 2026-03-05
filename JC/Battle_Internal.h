#pragma once

#include "JC/Common.h"

namespace JC::Draw   { DefHandle(Sprite); }
namespace JC::Unit   { DefHandle(Unit); enum struct Side; }
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
	Unit::Unit*    unit;
};

struct MoveCostMap {
	U32  moveCosts[MaxCols * MaxRows];
	Hex* parents[MaxCols * MaxRows];
};

struct Path {
	Hex* hexes[MaxCols * MaxRows];
	U32  len;
};

struct Data {
	Hex*        hexes;
	U32         hexesLen;
	Hex*        hoverHex;
	Hex*        selectedHex;
	MoveCostMap selectedHexMoveCostMap;
	Path        selectedHexToHoverHexPath;

};

//--------------------------------------------------------------------------------------------------

// Battle_Draw.cpp
Res<>  InitDraw(Mem tempMem, Window::State const* windowState);
void   MoveCamera();
void   Draw(Data const* data);

// Battle_Input.cpp
void Init();

// Battle_Path.cpp
void   InitPath(Mem permMem);
void   BuildMoveCostMap(Data const* data, Hex const* startHex, U32 move, Unit::Side side, MoveCostMap* moveCostMap);

// Battle_Util.cpp
void   InitUtil();
Vec2   ColRowToTopLeftWorldPos(I32 c, I32 r);
Vec2   HexToTopLeftWorldPos(Hex const* hex);
Vec2   HexToCenterWorldPos(Hex const* hex);
Hex*   WorldPosToHex(Data const* data, Vec2 p);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle