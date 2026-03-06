#pragma once

#include "JC/Common.h"

namespace JC::Particle {

//--------------------------------------------------------------------------------------------------

struct Emitter;
struct Type;

Type    CreateType();
Emitter CreateEmitter();
void    DestroyEmitter();
void    Update(F32 sec);
void    Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Particle