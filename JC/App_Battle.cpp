#pragma warning(disable: 4505)

#include "JC/App.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Event.h"
#include "JC/HandlePool.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Rng.h"
#include "JC/Time.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr I32 MapCols = 16;
static constexpr I32 MapRows = 12;
static constexpr U32 MaxUnitDefs = 64;
static constexpr U32 MaxUnits = 1024;

DefHandle(Unit);

struct HexPos {
	I32 col = 0;
	I32 row = 0;
};

struct UnitDef {
	Draw::Sprite sprite;
	U32          maxHp = 0;
};

struct UnitObj {
	UnitDef const * unitDef;
	U32             hp = 0;
};

enum struct TerrainType {
	Invalid  = 0,
	Grass,
	Forest,
	Mountain,
	Hill,
	Swamp,
	Max,
};

struct MapTile {
	HexPos      pos;
	TerrainType terrainType = TerrainType::Invalid;
	Unit        unit;
};

static Mem                       permMem;
static Mem                       tempMem;
static U8                        hexLut[32 * 24];
static Draw::Canvas              canvas;
static Vec2                      canvasSize(MapCols * 64 + 32, MapRows * 48 + 16);
static Vec2                      canvasTopLeft(25.f, 25.f);
static F32                       canvasScale = 2.f;
static Draw::Sprite              terrainSprites[(U32)TerrainType::Max];
static Draw::Sprite              terrainHighlightSprite;
static MapTile                   mapTiles[MapCols * MapRows];
static Vec2                      mousePos;
static HexPos                    mouseHexPos;
static UnitDef                   unitDefs[MaxUnitDefs];
static U32                       unitDefsLen;
static UnitDef*                  spearmenUnitDef;
static HandlePool<UnitObj, Unit> units;

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

//--------------------------------------------------------------------------------------------------

static Vec2 HexToCenterPixel(HexPos p) {
	return {
		(F32)(32 + p.col * 64 + (p.row & 1) * 32),
		(F32)(32 + p.row * 48),
	};
}

//---------------------------------------------------------------------------------------------

static HexPos PixelToHex(Vec2 p) {
	I32 iy = (I32)p.y;
	I32 row = iy / 48;
	const U8 parity = (row & 1);

	I32 ix = (I32)p.x - parity * 32;
	I32 col = ix / 64;
	if (ix < 0 || iy < 0) {
		return HexPos { .col = -1, .row = -1 };
	}

	I32 lx = ix - (col * 64);
	I32 ly = iy - (row * 48);
	Assert(lx >= 0 && ly >= 0);

	const U8 l = (hexLut[(ly / 2) * 32 + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;
	switch (l) {
		case 1: col -= 1 * (1 - parity); row -= 1; break;
		case 2: col += 1 * parity;       row -= 1; break;
	}
	return HexPos { .col = col, .row = row };
}


//--------------------------------------------------------------------------------------------------

Res<> PreInit(Mem permMemIn, Mem tempMemIn) {
	permMem = permMemIn;
	tempMem = tempMemIn;
	Cfg::SetStr(App::Cfg_Title, "4x Fantasy");
	Cfg::SetU32(App::Cfg_WindowWidth, 1600);
	Cfg::SetU32(App::Cfg_WindowHeight, 1200);
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Init(const Window::State*) {	// windowState
	InitHexLut();

	TryTo(Draw::CreateCanvas((U32)canvasSize.x, (U32)canvasSize.y), canvas);

	Try(Draw::LoadSpriteAtlas("Assets/Terrain.png", "Assets/Terrain.atlas"));
	Try(Draw::LoadSpriteAtlas("Assets/Units.png", "Assets/Units.atlas"));
	Try(Gpu::ImmediateWait());

	spearmenUnitDef = &unitDefs[unitDefsLen++];
	spearmenUnitDef->maxHp = 10;
	TryTo(Draw::GetSprite("Unit_Spearmen"), spearmenUnitDef->sprite);

	units.Init(permMem, MaxUnits);

	TryTo(Draw::GetSprite("Terrain_Grass"),     terrainSprites[(U32)TerrainType::Grass]);
	TryTo(Draw::GetSprite("Terrain_Forest"),    terrainSprites[(U32)TerrainType::Forest]);
	TryTo(Draw::GetSprite("Terrain_Swamp"),     terrainSprites[(U32)TerrainType::Swamp]);
	TryTo(Draw::GetSprite("Terrain_Hill"),      terrainSprites[(U32)TerrainType::Hill]);
	TryTo(Draw::GetSprite("Terrain_Mountain"),  terrainSprites[(U32)TerrainType::Mountain]);
	TryTo(Draw::GetSprite("Terrain_Highlight"), terrainHighlightSprite);

	for (I32 c = 0; c < MapCols; c++) {
		for (I32 r = 0; r < MapRows; r++) {
			auto const entry = units.Alloc();
			entry->obj.unitDef = spearmenUnitDef;
			entry->obj.hp = spearmenUnitDef->maxHp;
			mapTiles[c + r * MapCols] = {
				.pos         = { .col = c, .row = r },
				.terrainType = (TerrainType)Rng::NextU32(1, (U32)TerrainType::Max),
				.unit        = entry->Handle(),
			};
		}
	}

	mousePos = Vec2(0.f, 0.f);
	const Vec2 canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
	mouseHexPos = PixelToHex(canvasMousePos);


	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Update(U64 ticks) {
	const F32 secs = (F32)Time::Secs(ticks);
	secs;

	bool mouseMoved = false;
	for (Event::Event event; Event::GetEvent(&event);) {
		switch (event.type) {
			case Event::Type::ExitRequest:
				App::RequestExit();
				break;

			case Event::Type::WindowResized:
				Try(Draw::ResizeWindow(event.windowResized.width, event.windowResized.height));
				break;

			case Event::Type::Key:
				switch (event.key.key) {
					case Event::Key::Escape:
						App::RequestExit();
						break;

					case Event::Key::MouseLeft:
						if (Event::IsKeyDown(Event::Key::CtrlLeft) || Event::IsKeyDown(Event::Key::CtrlRight)) {
							if (mouseHexPos.col >= 0 && mouseHexPos.col < MapCols && mouseHexPos.row >= 0 && mouseHexPos.row < MapRows) {
								MapTile* const mapTile = &mapTiles[mouseHexPos.col + mouseHexPos.row * MapCols];
								if (!mapTile->unit) {
									auto const entry = units.Alloc();
									entry->obj.unitDef = spearmenUnitDef;
									entry->obj.hp = spearmenUnitDef->maxHp;
									mapTile->unit = entry->Handle();
								}
							}
						}
						break;
				}
				break;

			case Event::Type::MouseMove:
				mousePos.x = (F32)event.mouseMove.x;
				mousePos.y = (F32)event.mouseMove.y;
				mouseMoved = true;
				break;
		}
	}

	bool canvasScaleChanged = false;
	if (Event::IsKeyDown(Event::Key::Down)) { canvasScale *= 0.9995f; canvasScaleChanged = true; }
	if (Event::IsKeyDown(Event::Key::Up)) { canvasScale *= 1.0005f; canvasScaleChanged = true; }

	if (mouseMoved || canvasScaleChanged) {
		const Vec2 canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
		mouseHexPos = PixelToHex(canvasMousePos);
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw(Gpu::Frame const* gpuFrame) {
	Draw::BeginFrame(gpuFrame);

	//Draw::SetCanvas(canvas);
	//constexpr Vec4 bgColor(0.5f, 0.6f, 0.9f, 1.0f);
	//Draw::Draw({
	//	.pos   = Vec2(canvasSize.x / 2.f, canvasSize.y / 2.f),
	//	.size  = canvasSize,
	//	.color = bgColor,
	//});

	for (I32 col = 0; col < MapCols; col++) {
		for (I32 row = 0; row < MapRows; row++) {
			MapTile const* const mapTile = &mapTiles[col + row * MapCols];
			Vec2 const hexCenterPos = HexToCenterPixel(mapTile->pos);
			//Draw::Draw({
			//	.sprite = terrainSprites[(U32)mapTile->terrainType],
			//	.pos    = hexCenterPos,
			//});					

			if (mapTile->unit) {
				UnitObj const* const unitObj = units.Get(mapTile->unit);
				constexpr F32 UnitScale = 1.f;

				F32 outlineWidth = 0.f;
				if (mouseHexPos.col == col && mouseHexPos.row == row) {
					outlineWidth = 1.f;
				}
				Draw::Draw({
					.sprite       = unitObj->unitDef->sprite,
					.pos          = hexCenterPos,
					.size         = Vec2(UnitScale, UnitScale),
					.outlineColor = Vec4(1.f, 0.f, 0.f, 1.f),
					.outlineWidth = outlineWidth,
				});
			}
		}
	}
	
	//if (mouseHexPos.col >= 0 && mouseHexPos.col < MapCols && mouseHexPos.row >= 0 && mouseHexPos.row < MapRows) {
	//	Draw::Draw({
	//		.sprite = terrainHighlightSprite,
	//		.pos    = HexToCenterPixel(mouseHexPos),
	//	});
	//}

	//Draw::SetDefaultCanvas();
	//const Vec2 canvasCenter(canvasTopLeft.x + (canvasSize.x * canvasScale) / 2.f, canvasTopLeft.y + (canvasSize.y * canvasScale) / 2.f);
	//Draw::Draw({
	//	.canvas = canvas,
	//	.pos    = canvasCenter,
	//	.size   = Vec2(canvasScale, canvasScale),
	//});

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