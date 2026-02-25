#pragma once

#include "JC/Common.h"

namespace JC::Gpu { struct Frame; };

namespace JC::Draw {

//--------------------------------------------------------------------------------------------------

DefHandle(Sprite);
DefHandle(Canvas);

struct InitDesc {
	Mem permMem;
	Mem tempMem;
	U32 windowWidth;
	U32 windowHeight;
};

struct DrawDesc {
	Sprite sprite;
	Canvas canvas;
	Vec2   pos;
	F32    z = 0.f;
	Vec2   size         = Vec2(1.f, 1.f);
	Vec4   color        = Vec4(1.f, 1.f, 1.f, 1.f);
	Vec4   outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
	F32    outlineWidth = 0.f;
	F32    rotation     = 0.f;
};

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
void        Draw(DrawDesc drawDesc);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw