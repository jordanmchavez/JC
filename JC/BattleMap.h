#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::BMap {

//--------------------------------------------------------------------------------------------------

DefHandle(Terrain);
DefHandle(Hex);

struct HexDef {
	Terrain terrain;
	HexPos  pos;
};

//--------------------------------------------------------------------------------------------------

void         Init(Mem permMem, Mem tempMem);
Res<>        LoadBattleMapJson(Str battleMapJJsonPath);
Res<Terrain> GetTerrain(Str name);
Hex          CreateHex(Terrain terrain, HexPos pos);
void         DestroyHex(Hex hex);
HexPos       GetHexPos(Hex hex);
U32          GetHexMoveCost(Hex hex);
Hex          GetHexByWorldPos(Vec2 worldPos);
void         ShowHexBorder(Hex hex, Vec4 color);
void         HideHexBorder(Hex hex);
void         ShowHexHighlight(Hex hex, Vec4 color);
void         HideHexHighlight(Hex hex);
void         Draw(F32 z);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::BMap