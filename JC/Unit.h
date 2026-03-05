#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxUnitAnimationFrames = 16;
static constexpr U32 MaxUnitAnimations      = 16;

namespace Side {
	enum Enum {
		Left = 0,
		Right,
		Max
	};
};

struct UnitDef {
	Str           name;
	Draw::Sprite  sprite;
	Vec2          size;
	U32           hp;
	U32           move;
};

struct Unit {
	UnitDef const* unitDef;
	Side::Enum     side;
	Vec2           pos;
	U32            hp;
	U32            move;
	Unit*          next;
	U32            nextFreeIdx;
};

void                Init(Mem permMem, Mem tempMem);
Res<>               LoadUnitDefsJson(Str unitDefJsonPath);
Res<UnitDef const*> GetUnitDef(Str name);
Unit*               AllocUnit();
void                FreeUnit(Unit* unit);
void                Frame(F32 sec);
void                Draw(F32 z);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit