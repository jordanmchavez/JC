#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

namespace StatFlags {
	constexpr U64 Resource            = (U64)1 << 0;
	constexpr U64 ReplenishesEachTurn = (U64)1 << 1;
};

struct StatType {
	Str          name;
	Draw::Sprite sprite;
	U64          flags;
};

struct Stat {
	StatType const* statType;
	U32             max;
	U32             cur;
};

struct DamageType {
	Str          name;
	Draw::Sprite sprite;
	StatType*    defensStatType;
};

struct AttackCost {
	StatType const* statType;
	U32             cost;
};

struct Attack {
	Str                name;
	Draw::Sprite       sprite;
	DamageType const*  damageType;
	U32                damage;
	U32                counterDamage;
	Array<AttackCost>  costs;
};

struct Unit {
	Str           name;
	Draw::Sprite  sprite;
	Vec2          pos;
	Array<Stat>   stats;
	Array<Attack> attacks;
};

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMem);
Res<> Load(Str path);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit