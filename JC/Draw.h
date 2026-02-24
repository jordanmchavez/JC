#pragma once

#include "JC/Common.h"

namespace JC::Gpu { struct Frame; };

namespace JC::Draw {

//--------------------------------------------------------------------------------------------------

struct InitDesc {
	Mem permMem;
	Mem tempMem;
	U32 windowWidth;
	U32 windowHeight;
};

DefHandle(Sprite);
DefHandle(Canvas);

Res<>       Init(InitDesc const* initDesc);
void        Shutdown();
Res<>       ResizeWindow(U32 width, U32 height);
Res<>       LoadSpriteAtlas(Str imagePath, Str atlasPath);
Res<Sprite> GetSprite(Str name);
Vec2        GetSpriteSize(Sprite sprite);
Res<Canvas> CreateCanvas(U32 width, U32 height);
void        DestroyCanvas(Canvas canvas);
void        BeginFrame(const Gpu::Frame* frame);
void        EndFrame();
void        SetDefaultCanvas();
void        SetCanvas(Canvas canvas);
void        DrawSprite(Sprite sprite, Vec2 pos);
void        DrawSprite(Sprite sprite, Vec2 pos, Vec2 scale, F32 rotation, Vec4 color);
void        DrawRect(Vec2 pos, Vec2 size, Vec4 color);
void        DrawCanvas(Canvas canvas, Vec2 pos, Vec2 scale);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw