#pragma once

#include "JC/Common.h"

namespace JC::App    { struct FrameData; }
namespace JC::Draw   { DefHandle(Sprite); }
namespace JC::Unit   { struct Unit; }
namespace JC::Window { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

constexpr I32 MaxRows = 16;
constexpr I32 MaxCols = 16;
constexpr U32 MaxPath = MaxRows * MaxCols;

struct Terrain {
	Str          name;
	Draw::Sprite sprite;
	U32          moveCost;
};

struct Hex {
	U32            idx;
	HexPos         hexPos;
	Terrain const* terrain;
	Unit::Unit*    unit;
	bool           border;
	Vec4           borderColor;
	bool           highlight;
	Vec4           highlightColor;
	Hex*           neighbors[6];
	U32            neighborsLen;
};

struct Path {
	Hex* hexes[MaxPath];
	U32  len;
};

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMem, Window::State const* windowState);
Res<> LoadBattleJson(Str battleJsonPath);
Res<> GenerateMap();
Res<> Frame(App::FrameData const* appFrameData);
void  Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle