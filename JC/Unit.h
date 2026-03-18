#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Sprite); }

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefHandle(ResourceType);

void         Init(Mem permMem);
Res<>        InitDefs();
ResourceType CreateResourceType(Str name, Draw::Sprite sprite);
ResourceType GetResourceType(Str name);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit