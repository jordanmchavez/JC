#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefHandle(Def);
DefHandle(Unit);

enum struct Side { Left, Right };

struct DefData {
	Str name;
	U32 hp;
	U32 move;
};

struct Data {
	DefData const* defData;
	Side           side;
	U32            hp;
	U32            move;
};

void     Init(Mem permMem, Mem tempMem);
Res<>    LoadDefs(Str path);
Res<Def> GetUnitDef(Str name);
Unit     CreateUnit(Def def);
void     FreeUnit(Unit* unit);
void     Frame(F32 sec);
void     Draw(F32 z);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit