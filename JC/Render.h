#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; }

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

DefErr(Render, SkipFrame);

//--------------------------------------------------------------------------------------------------

struct InitDesc {
	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;
	U32                 windowWidth   = 0;
	U32                 windowHeight  = 0;
};

struct Sprite { U64 handle = 0; };

struct DrawSprite {
	Sprite sprite;
	Vec3   xyz;
};

//--------------------------------------------------------------------------------------------------

Res<>       Init(const InitDesc* initDesc);
void        Shutdown();
Res<>       WindowResized(U32 windowWidth, U32 windowHeight);

Res<>       LoadSpriteAtlas(Str imagePath, Str atlasPath);
Res<Sprite> GetSprite(Str name);

void        SetProjView(const Mat4* projView);

Res<>       BeginFrame();
Res<>       EndFrame();
void        DrawSprites(Span<DrawSprite> drawSprites);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render