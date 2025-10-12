#pragma once

#include "JC/Common_Res.h"
#include "JC/Common_Std.h"

namespace JC::Gpu { struct Image; }

namespace JC::Sprite {

//--------------------------------------------------------------------------------------------------

struct Sprite {
	Gpu::Image image;
	Vec2       uv1;
};

Res<>       LoadAtlas(Str imagePath, Str atlasPath);
Res<Sprite> GetSprite(Str name);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sprite