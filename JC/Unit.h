#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxUnitAnimationFrames = 16;
static constexpr U32 MaxUnitAnimations      = 16;

namespace Side {
	enum Enum {
		Invalid = 0,
		Left,
		Right,
		Max
	};
};

struct UnitAnimationFrame {
	Draw::Sprite sprite;
	F32          sec;
};

struct UnitAnimation {
	UnitAnimationFrame frames[MaxUnitAnimationFrames];
	U32                framesLen;
};

struct UnitDef {
	Str           name;
	Draw::Sprite  sprite;
	Vec2          size;
	U32           maxHp;
	U32           maxMove;
	U32           damage;
	UnitAnimation animations[MaxUnitAnimations];
	U32           animationsLen;
};

struct Unit {
	UnitDef const* def;
	Side::Enum     side;
	Vec2           pos;
	U32            hp;
	U32            move;
	U32            activeAnimationIdx;
	F32            activeAnimationSec;
	bool           outline;
	Unit*          next;
};

Res<>          LoadUnitFile(Str unitFilePath);
UnitDef const* GetUnitDef(Str name);
Unit*          CreateUnit(UnitDef unitDef, Side::Enum side, Vec2 pos);
void           DestroyUnit(Unit* unit);
void           Frame(F32 sec);
void           Draw(F32 z);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit