#pragma once

#include "JC/Common.h"

namespace JC::Gpu { struct Frame; };

namespace JC::Draw {

//--------------------------------------------------------------------------------------------------

DefHandle(Sprite);
DefHandle(Font);
DefHandle(Canvas);

struct InitDef {
	Mem permMem;
	Mem tempMem;
	U32 windowWidth;
	U32 windowHeight;
};

enum struct Origin {
	TopLeft,
	TopCenter,
	TopRight,
	Left,		// BaselineLeft for fonts
	Center,		// BaselineCenter for fonts
	Right,		// BaselineRight for fonts
	BottomLeft,
	BottomCenter,
	BottomRight,
};

struct SpriteDrawDef {
	Sprite sprite;
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Center;
	Vec2   scale = Vec2(1.f, 1.f);
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
	Vec4   outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
	F32    outlineWidth = 0.f;
	F32    rotation = 0.f;
};

struct FontDrawDef {
	Font   font;
	Str    str;
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Left;
	Vec2   scale = Vec2(1.f, 1.f);
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
};

struct CanvasDrawDef {
	Canvas canvas;
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Center;
	Vec2   scale = Vec2(1.f, 1.f);
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
	F32    rotation = 0.f;
};

struct RectDrawDef {
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Center;
	Vec2   size;
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
};

Res<>       Init(InitDef const* initDef);
void        Shutdown();
Res<>       ResizeWindow(U32 width, U32 height);
Res<>       LoadSprites(Str imagePath, Str spritesPath);
Res<Sprite> GetSprite(Str name);
Vec2        GetSpriteSize(Sprite sprite);
Res<Font>   LoadFont(Str fontPath, Str imagePath);
F32         GetFontLineHeight(Font font);
Res<Canvas> CreateCanvas(U32 width, U32 height);
void        DestroyCanvas(Canvas canvas);
void        BeginFrame(Gpu::Frame const* frame);
void        EndFrame();
void        SetDefaultCanvas();
void        SetCanvas(Canvas canvas);
void        DrawSprite(SpriteDrawDef drawDef);
void        DrawFont(FontDrawDef drawDef);
void        DrawCanvas(CanvasDrawDef drawDef);
void        DrawRect(RectDrawDef drawDef);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw