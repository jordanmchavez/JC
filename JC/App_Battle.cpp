#pragma once

#include "JC/App.h"
#include "JC/Draw.h"
#include "JC/Event.h"
#include "JC/Gpu.h"
#include "JC/Math.h"
#include "JC/Rng.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MapCols = 16;
static constexpr U32 MapRows = 12;

namespace Terrain {
	using Type = U32;
	constexpr U32 Grass    = 1;
	constexpr U32 Forest   = 2;
	constexpr U32 Mountain = 3;
	constexpr U32 Hill     = 4;
	constexpr U32 Swamp    = 5;
	constexpr U32 Max      = 6;
};

struct MapPos {
	U32 col;
	U32 row;
};

static Mem           permMem;
static Mem           tempMem;
static U8            hexLut[32 * 24];
static Draw::Canvas  canvas;
static U32           canvasWidth = MapCols * 64;
static U32           canvasHeight = 64 + (MapRows - 1) * 48;
static Vec2          canvasPos = { .x = 50.f, .y = 50.f };
static F32           canvasScale = 2.f;
static Draw::Sprite  terrainSprites[Terrain::Max];
static Terrain::Type mapTiles[MapCols * MapRows];

//--------------------------------------------------------------------------------------------------

static void InitHexLut() {
	memset(hexLut, 0, sizeof(hexLut));
	U8 start = 31;
	U8 end   = 32;
	for (U8 y = 0; y < 16; y++) {
		for (U8 x = 0; x < start; x++) {
			hexLut[((y / 2) * 32) + (x / 2)] |= (1 << ((x & 1) * 4));
		}
		for (U8 x = end + 1; x < 64; x++) {
			hexLut[((y / 2) * 32) + (x / 2)] |= (2 << ((x & 1) * 4));
		}
		start -= 2;
		end   += 2;
	}
	return;
}
/*
static Vec2 HexToPixel(MapPos p) {
	return {
		(F32)(32 + p.col * 64 + (p.row & 1) * 32),
		(F32)(32 + p.row * 48),
	};
}

static MapPos PixelToHex(Vec2 p) {
	p = Math::Sub(p, canvasPos);
	p = Math::Scale(p, 1.f / canvasScale);
	//pos = Math::Sub(pos, Vec2(MapPadding, MapPadding));

	I32 iy = (I32)p.y;
	I32 row = iy / 48;
	const U8 parity = (row & 1);

	I32 ix = (I32)p.x - parity * 32;
	I32 col = ix / 64;
	if (ix < 0 || iy < 0) {
		return { .col = U32Max, .row = U32Max };
	}

	I32 lx = ix - (col * 64);
	I32 ly = iy - (row * 48);
	Assert(lx >= 0 && ly >= 0);

	const U8 l = (hexLut[(ly / 2) * 32 + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;

	switch (l) {
		case 1: col -= 1 * (1 - parity); row -= 1; break;
		case 2: col += 1 * parity;       row -= 1; break;
	}
	if (col >= (I32)MapCols || row >= (I32)MapRows) {
		return { .col = U32Max, .row = U32Max };
	}

	return { .col = (U32)col, .row = (U32)row };
}
*/
//--------------------------------------------------------------------------------------------------

Res<> PreInit(Mem permMemIn, Mem tempMemIn) {
	permMem = permMemIn;
	tempMem = tempMemIn;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Init(const Window::State*) {	// windowState
	InitHexLut();
	TryTo(Draw::CreateCanvas(canvasWidth, canvasHeight), canvas);
	Try(Draw::LoadSpriteAtlas("Assets/Terrain.png", "Assets/Terrain.atlas"));
	Try(Gpu::ImmediateWait());
	TryTo(Draw::GetSprite("Terrain_Grass"),    terrainSprites[Terrain::Grass]);
	TryTo(Draw::GetSprite("Terrain_Forest"),   terrainSprites[Terrain::Forest]);
	TryTo(Draw::GetSprite("Terrain_Swamp"),    terrainSprites[Terrain::Swamp]);
	TryTo(Draw::GetSprite("Terrain_Hill"),     terrainSprites[Terrain::Hill]);
	TryTo(Draw::GetSprite("Terrain_Mountain"), terrainSprites[Terrain::Mountain]);

	for (U32 c = 0; c < MapCols; c++) {
		for (U32 r = 0; r < MapRows; r++) {
			mapTiles[c * MapRows + r] = Rng::NextU32(1, Terrain::Max);
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Update(U64 ticks) {
	ticks;

	for (Event::Event event; Event::GetEvent(&event);) {
		if (event.type == Event::Type::ExitRequest) {
			App::RequestExit();
		} else if (event.type == Event::Type::Key) {
			if (event.key.key == Event::Key::Escape) { App::RequestExit(); }
		} else if (event.type == Event::Type::WindowResized) {
			Try(Draw::ResizeWindow(event.windowResized.width, event.windowResized.height));
		}
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw(const Gpu::Frame* gpuFrame) {
	Draw::BeginFrame(gpuFrame);

	Draw::SetCanvas(canvas);
	Draw::DrawRect(Vec2(0.f, 0.f), Vec2((F32)canvasWidth, (F32)canvasHeight), Vec4(0.15f, 0.3f, 0.25f, 1.0f));

	for (U32 col = 0; col < MapCols; col++) {
		for (U32 row = 0; row < MapRows; row++) {
			Draw::DrawSprite(
				terrainSprites[mapTiles[col * MapRows + row]],
				Vec2 {
					.x = (F32)((col * 64) + ((row & 1) * -32)),
					.y = (F32)(row* 48),
				}
			);
		}
	}

	Draw::SetCanvas();
	Draw::DrawCanvas(canvas, canvasPos, Vec2 { .x = canvasScale, .y = canvasScale });

	Draw::EndFrame();

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	Draw::DestroyCanvas(canvas);
}

//--------------------------------------------------------------------------------------------------

App::App app = {
	.PreInit  = PreInit,
	.Init     = Init,
	.Shutdown = Shutdown,
	.Update   = Update,
	.Draw     = Draw,
};

App::App* GetApp() { return &app; }

}	// namespace JC::Battle