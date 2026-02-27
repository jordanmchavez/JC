#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Effect {

//--------------------------------------------------------------------------------------------------

struct EffectDef {
	// ?? fill me in
};

void Create(const EffectDef* effectDef);
void Update(U64 ticks);
void Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Effect