#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMem);
Res<> Load(Str path);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit