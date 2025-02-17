#pragma once

#include "JC/Core.h"

namespace JC::Sprite {

//--------------------------------------------------------------------------------------------------

struct Sprite { u64 handle = 0; };

Res<>  Load(Str imagePath, Str atlasPath);
Sprite Get(Str name);
Res<>  Draw(Span<Sprite> sprites);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sprite