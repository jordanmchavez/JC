#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::UnitDef {

//--------------------------------------------------------------------------------------------------

struct Def {
	Str          name;
	Draw::Sprite sprite;
	Vec2         size;
	U32          hp;
	U32          move;
};

void            Init(Mem permMem, Mem tempMem);
Res<>           LoadDefs(Str path);
Res<Def const*> GetDef(Str name);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::UnitDef