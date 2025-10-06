#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

struct Gpu_Frame;

struct Drw_InitDesc {
	U32 windowWidth   = 0;
	U32 windowHeight  = 0;
};

struct Drw_Sprite { U64 handle = 0; };
struct Drw_Canvas { U64 handle = 0; };

Res<>           Drw_Init(Drw_InitDesc const* initDesc);
void            Drw_Shutdown();
Res<>           Drw_ResizeWindow(U32 windowWidth, U32 windowHeight);
Res<>           Drw_LoadSpriteAtlas(Str imagePath, Str atlasPath);
Res<Drw_Sprite> Drw_GetSprite(Str name);
Vec2            Drw_GetSpriteSize(Drw_Sprite sprite);
Res<Drw_Canvas> Drw_CreateCanvas(U32 width, U32 height);
void            Drw_DestroyCanvas(Drw_Canvas canvas);
void            Drw_BeginFrame(Gpu_Frame* frame);
void            Drw_EndFrame();
void            Drw_SetCanvas(Drw_Canvas canvas = Drw_Canvas());
void            Drw_DrawSprite(Drw_Sprite sprite, Vec2 pos);
void            Drw_DrawSprite(Drw_Sprite sprite, Vec2 pos, Vec2 scale, F32 rotation, Vec4 color);
void            Drw_DrawRect(Vec2 pos, Vec2 size, Vec4 color, Vec4 borderColor, F32 border, F32 cornerRadius);
void            Drw_DrawCanvas(Drw_Canvas canvas, Vec2 pos, Vec2 scale);

/*
Gpu = GPU
Drw = Draw
Ren = Renderer
Evt = event
Err = err
Fmt
Jsn
Hsh
Wnd
Uni
Tim
Sys
Rng
Mem
Log
Prs
Cli
Srv
Fmt
Com

EventApi
*/

struct EventApi {
	
};