#pragma once

#include "JC/Common.h"

namespace JC::Particle {

//--------------------------------------------------------------------------------------------------

struct Emitter;
struct Type;

Type    CreateType();
Emitter CreateEmitter();
void    DestroyEmitter();
void    Frame(U64 ticks);
void    Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Particle