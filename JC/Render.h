#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; }

namespace JC::Render {

struct InitDesc {
	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;
	U32                 windowWidth   = 0;
	U32                 windowHeight  = 0;
};

struct Sprite { U64 handle = 0; };

Res<>  Init(const InitDesc* initDesc);
void   Shutdown();
Res<>  LoadSpriteAtlas(Str imgPath, Str atlasPath);
Res<>  BeginFrame();
void   EndFrame();
Sprite GetSprite(Str name);
void   DrawSprites(Span<Sprite> Sprites);

Res<> WindowResized(U32 windowWidth, U32 windowHeight) {
	if (Res<> r = Gpu::RecreateSwapchain(windowState.width, windowState.height); !r) {
		return r;
	}
}

}	// namespace JC::Render