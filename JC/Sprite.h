#pragma once

#include "JC/Core.h"

namespace JC::Sprite {

//--------------------------------------------------------------------------------------------------

struct Sprite { U64 handle = 0; };

struct DrawSprite {
	Sprite sprite;
};

Res<>  LoadAtlas(Str imagePath, Str atlasPath);
Sprite Get(Str name);
Res<>  Draw(Span<DrawSprite> sprites);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sprite