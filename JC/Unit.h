#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxUnitAnimationFrames = 16;
static constexpr U32 MaxUnitAnimations = 16;

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
	U32           damage;
	UnitAnimation animations[MaxUnitAnimations];
	U32           animationsLen;
};

struct Unit {
	UnitDef const* unitDef;
	U32            hp;
	Vec2           pos;
	U32            activeAnimationIdx;
	F32            activeAnimationSec;
	U32            outlineSize;
	Vec4           outlineColor;
};

Res<>          LoadUnitFile(Str unitFilePath);
UnitDef const* GetUnitDef(Str name);
Unit*          CreateUnit(UnitDef unitDef);
void           DestroyUnit(Unit* unit);
void           Frame(F32 sec);
void           Draw(F32 z);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit