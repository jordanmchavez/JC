#pragma once

#include "JC/Common.h"

namespace JC::Gpu { struct FrameData; };

namespace JC::Draw {

//--------------------------------------------------------------------------------------------------

DefHandle(Sprite);
DefHandle(Font);
DefHandle(Canvas);

struct InitDesc {
	Mem permMem;
	Mem tempMem;
	U32 windowWidth;
	U32 windowHeight;
};

struct Camera {
	Vec2 pos;
	F32  scale;
};

enum struct Origin {
	TopLeft,
	TopCenter,
	TopRight,
	Left,
	Center,
	Right,
	BottomLeft,
	BottomCenter,
	BottomRight,

	// only valid for StrDrawDesc
	BaselineLeft,
	BaselineCenter,
	BaselineRight
};

struct DrawRectDesc {
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Center;
	Vec2   size;
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
};

struct DrawSpriteDesc {
	Sprite sprite;
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Center;
	Vec2   scale = Vec2(1.f, 1.f);
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
	bool   flip = false;
	F32    outlineWidth = 0.f;
	Vec4   outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
};

struct DrawStrDesc {
	Font   font;
	Str    str;
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Left;
	Vec2   scale = Vec2(1.f, 1.f);
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
	F32    outlineWidth = 0.f;
	Vec4   outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
};

struct DrawCanvasDesc {
	Canvas canvas;
	Vec2   pos;
	F32    z;
	Origin origin = Origin::Center;
	Vec2   scale = Vec2(1.f, 1.f);
	Vec4   color = Vec4(1.f, 1.f, 1.f, 1.f);
};

Res<>       Init(InitDesc const* initDesc);
void        Shutdown();
Res<>       ResizeWindow(U32 width, U32 height);
Res<>       LoadSprites(Str path);
Res<Sprite> GetSprite(Str name);
Vec2        GetSpriteSize(Sprite sprite);
Res<>       LoadFont(Str path);
Res<Font>   GetFont(Str name);
F32         GetFontLineHeight(Font font);
Res<Canvas> CreateCanvas(U32 width, U32 height);
void        BeginFrame(Gpu::FrameData const* gpuFrameData);
void        EndFrame();
void        SetDefaultCanvas();
void        SetCanvas(Canvas canvas);
void        SetCamera(Camera camera);
void        ClearCamera();
void        DrawRect(DrawRectDesc drawRectDesc);
void        DrawSprite(DrawSpriteDesc drawSpriteDesc);
void        DrawStr(DrawStrDesc drawStrDesc);
void        DrawCanvas(DrawCanvasDesc drawCanvasDesc);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw