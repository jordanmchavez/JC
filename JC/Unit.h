#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefHandle(Def);
DefHandle(Unit);

namespace Side {
	constexpr U32 Left  = 0;
	constexpr U32 Right = 1;
}

struct DefData {
	Str          name;
	Draw::Sprite sprite;
	Vec2         size;
	U32          hp;
	U32          move;
};

struct Data {
	U32 side;
	U32 hp;
};

void           Init(Mem permMem, Mem tempMem);
Res<>          LoadDefs(Str path);
Res<Def>       GetDef(Str name);
DefData const* GetDefData(Def def);
Unit           CreateUnit(Def def, U32 side);
void           DestroyUnit(Unit unit);
Data*          GetData(Unit unit);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit