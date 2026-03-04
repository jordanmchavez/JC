#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::BMap {

//--------------------------------------------------------------------------------------------------

DefHandle(Terrain);
DefHandle(Hex);

struct HexPos {
	I32 col;
	I32 row;
};

struct HexDef {
	Terrain terrain;
	HexPos  pos;
};

//--------------------------------------------------------------------------------------------------

void    Init(Mem permMem);
Res<>   LoadBattleMapDef(Str battleMapDefPath);
Terrain GetTerrain(Str name);
Hex     CreateHex(HexDef hexDef);
void    DestroyHex(Hex hex);
Hex     GetHexByWorldPos(Vec2 worldPos);
HexPos  GetHexPos(Hex hex);
void    HighlightHex(Hex hex);
void    SelectHex(Hex hex);
void    Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::BMap