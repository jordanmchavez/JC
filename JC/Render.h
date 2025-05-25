#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; }

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

struct InitDesc {
	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;
	U32                 windowWidth   = 0;
	U32                 windowHeight  = 0;
};

struct Sprite { U64 handle = 0; };

Res<>       Init(const InitDesc* initDesc);
void        Shutdown();
Res<>       WindowResized(U32 windowWidth, U32 windowHeight);

Res<>       LoadSpriteAtlas(Str imagePath, Str atlasPath);
Res<Sprite> GetSprite(Str name);
Vec2        GetSpriteSize(Sprite sprite);

void        BeginFrame();
void        EndFrame();

void        DrawSprite(Sprite sprite, Vec2 pos);
void        DrawSprite(Sprite sprite, Vec2 pos, F32 size, F32 rotation, Vec4 color);
void        DrawRect(Vec2 pos, Vec2 size, Vec4 color, Vec4 borderColor, F32 border, F32 cornerRadius);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render