#pragma once

#include "JC/Common.h"

namespace JC::Effect {

//--------------------------------------------------------------------------------------------------

struct EffectDef {
	// ???
};

void Create(const EffectDef* effectDef);
void Update(U64 ticks);
void Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Effect