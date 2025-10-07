#pragma once

#include "JC/Common_Res.h"
#include "JC/Common_Std.h"

namespace JC::Gpu { struct Frame; };

namespace JC::Draw {

//--------------------------------------------------------------------------------------------------

struct InitDesc {
	U32 windowWidth   = 0;
	U32 windowHeight  = 0;
};

struct Sprite { U64 handle = 0; };
struct Canvas { U64 handle = 0; };

Res<>       Init(InitDesc const* initDesc);
void        Shutdown();
Res<>       ResizeWindow(U32 width, U32 height);
Res<>       LoadSpriteAtlas(Str imagePath, Str atlasPath);
Res<Sprite> GetSprite(Str name);
Vec2        GetSpriteSize(Sprite sprite);
Res<Canvas> CreateCanvas(U32 width, U32 height);
void        DestroyCanvas(Canvas canvas);
void        BeginFrame(Gpu::Frame* frame);
void        EndFrame();
void        SetCanvas(Canvas canvas = Canvas());
void        DrawSprite(Sprite sprite, Vec2 pos);
void        DrawSprite(Sprite sprite, Vec2 pos, Vec2 scale, F32 rotation, Vec4 color);
void        DrawRect(Vec2 pos, Vec2 size, Vec4 color, Vec4 borderColor, F32 border, F32 cornerRadius);
void        DrawCanvas(Canvas canvas, Vec2 pos, Vec2 scale);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw