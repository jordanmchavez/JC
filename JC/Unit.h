#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefHandle(Def);
DefHandle(Unit);

enum struct Side { Left, Right };

struct DefData {
	Str          name;
	Draw::Sprite sprite;
	Vec2         size;
	U32          hp;
	U32          move;
};

struct Data {
	Unit           unit;
	DefData const* defData;
	Side           side;
	Vec2           pos;
	U32            hp;
	U32            move;
};

void     Init(Mem permMem, Mem tempMem);
Res<>    LoadDefs(Str path);
Res<Def> GetDef(Str name);
Data*    CreateUnit(Def def, Side side,Vec2 pos);
void     DestroyUnit(Unit unit);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit