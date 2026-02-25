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

static constexpr I16 HexSize = 32;
static constexpr I32 MapCols = 16;
static constexpr I32 MapRows = 12;
static constexpr U32 MaxGrassFeatures = 9;
static constexpr U32 MaxTerrainFeatures = 32;
static constexpr U32 MaxUnitDefs = 64;
static constexpr U32 MaxUnits = 1024;


static constexpr Vec4 MapBackgroundColor = Vec4(13.f/255.f, 30.f/255.f, 22.f/255.f, 1.f);
static constexpr Vec4 SelectedColor = Vec4(0.f, 1.f, 0.f, 1.f);
static constexpr Vec4 HoverColor = Vec4(1.f, 1.f, 1.f, 0.75f);

DefHandle(Unit);

enum struct State {
	WaitingOrder,
	ExecutingOrder,
};

struct HexCoord {
	I16 x = 0;
	I16 y = 0;
};

struct MapCoord {
	I16 col = 0;
	I16 row = 0;
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

struct TerrainFeature {
	Draw::Sprite sprite;
	HexCoord     hexCoord;
};

struct MapTile {
	MapCoord       mapCoord;
	TerrainType    terrainType = TerrainType::Invalid;
	TerrainFeature terrainFeatures[MaxTerrainFeatures];
	U32            terrainFeaturesLen = 0;
	Unit           unit;
};

struct UnitDef {
	Draw::Sprite sprite;
	U32          maxHp = 0;
};

struct UnitObj {
	UnitDef const* unitDef = nullptr;
	U32            hp = 0;
	MapTile*       mapTile = nullptr;
};

static Mem                       permMem;
static Mem                       tempMem;
static U8                        hexLut[(HexSize / 2) * (HexSize * 3 / 8)];
static HexCoord                  hexCoordLut[(HexSize / 2) * (HexSize / 2)];
static U32                       hexCoordLutLen;
static Draw::Canvas              canvas;
static Vec2                      canvasSize(MapCols * HexSize + (HexSize / 2), MapRows * (HexSize * 3 / 4) + (HexSize / 4));
static Vec2                      canvasTopLeft(25.f, 25.f);
static MapTile                   mapTiles[MapCols * MapRows];
static Vec2                      mousePos;
static MapCoord                  mouseMapCoord;
static UnitDef                   unitDefs[MaxUnitDefs];
static U32                       unitDefsLen;
static UnitDef*                  spearmenUnitDef;
static HandlePool<UnitObj, Unit> units;
static State                     state;
static Draw::Sprite              hexSprite;
static Draw::Sprite              hexBorderSprite;
static Draw::Sprite              grassFeatureSprites[MaxGrassFeatures];
static MapTile*                  selectedMapTile;
static MapTile*                  hoverMapTile;

static F32 canvasScale = 2.f;
static F32 unitScale = 1.f;

//--------------------------------------------------------------------------------------------------

static void InitLuts() {
	memset(hexLut, 0, sizeof(hexLut));
	U8 start = (HexSize / 2) - 1;
	U8 end   = HexSize / 2;
	for (U8 y = 0; y < HexSize / 4; y++) {
		for (U8 x = 0; x < start; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (1 << ((x & 1) * 4));
		}
		for (U8 x = end + 1; x < HexSize; x++) {
			hexLut[((y / 2) * (HexSize / 2)) + (x / 2)] |= (2 << ((x & 1) * 4));
		}
		start -= 2;
		end   += 2;
	}

	hexCoordLutLen = 0;
	I16 xStart = (HexSize / 2) - 1;
	I16 y = 0;
	for (; y <= (HexSize / 4) - 1; y++) {
		for (I16 x = xStart; x < (HexSize / 2); x++) {
			hexCoordLut[hexCoordLutLen++] = {
				.x = x,
				.y = y,
			};
		}
		xStart -= 2;
	}
	for (; y < (HexSize / 2); y++) {
		for (I16 x = 0; x < (HexSize / 2); x++) {
			hexCoordLut[hexCoordLutLen++] = {
				.x = x,
				.y = y,
			};
		}
	}
}

HexCoord RngHexCoord() {
	HexCoord hc = hexCoordLut[Rng::NextU32(0, hexCoordLutLen)];
	const U32 flip = Rng::NextU32(0, 4);
    if (flip & 1) { hc.x = HexSize - hc.x; }
    if (flip & 2) { hc.y = HexSize - hc.y; }
	return hc;
}

//--------------------------------------------------------------------------------------------------

static Vec2 MapCoordToCenterPixel(MapCoord mc) {
	return {
		(F32)((HexSize / 2) + mc.col * HexSize + (mc.row & 1) * (HexSize / 2)),
		(F32)((HexSize / 2) + mc.row * (HexSize * 3 / 4)),
	};
}

//---------------------------------------------------------------------------------------------

static MapCoord PixelToMapCoord(Vec2 p) {
	I32 iy = (I32)p.y;
	I32 row = iy / (HexSize * 3 / 4);
	const U8 parity = (row & 1);

	I32 ix = (I32)p.x - parity * (HexSize / 2);
	I32 col = ix / HexSize;
	if (ix < 0 || iy < 0) {
		return MapCoord { .col = -1, .row = -1 };
	}

	I32 lx = ix - (col * HexSize);
	I32 ly = iy - (row * (HexSize * 3 / 4));
	Assert(lx >= 0 && ly >= 0);

	const U8 l = (hexLut[(ly / 2) * (HexSize / 2) + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;
	switch (l) {
		case 1: col -= 1 * (1 - parity); row -= 1; break;
		case 2: col += 1 * parity;       row -= 1; break;
	}
	return MapCoord { .col = (I16)col, .row = (I16)row };
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
	InitLuts();

	TryTo(Draw::CreateCanvas((U32)canvasSize.x, (U32)canvasSize.y), canvas);

	Try(Draw::LoadSpriteAtlas("Assets/Terrain.png", "Assets/Terrain.atlas"));
	Try(Draw::LoadSpriteAtlas("Assets/Units.png", "Assets/Units.atlas"));
	Try(Gpu::ImmediateWait());

	spearmenUnitDef = &unitDefs[unitDefsLen++];
	spearmenUnitDef->maxHp = 10;
	TryTo(Draw::GetSprite("Unit_Spearmen"), spearmenUnitDef->sprite);

	units.Init(permMem, MaxUnits);

	TryTo(Draw::GetSprite("Hex_32"), hexSprite);
	TryTo(Draw::GetSprite("Hex_32_Border"), hexBorderSprite);
	for (U32 i = 0; i < MaxGrassFeatures; i++) {
		TryTo(Draw::GetSprite(SPrintf(tempMem, "TerrainFeature_Grass_%u", i)), grassFeatureSprites[i]);
	}

	for (I16 c = 0; c < MapCols; c++) {
		for (I16 r = 0; r < MapRows; r++) {
			MapTile* const mapTile = &mapTiles[c + r * MapCols];
			auto const entry = units.Alloc();
			entry->obj.unitDef = spearmenUnitDef;
			entry->obj.hp      = spearmenUnitDef->maxHp;
			entry->obj.mapTile = mapTile;
			mapTile->mapCoord    = { .col = c, .row = r };
			mapTile->terrainType = (TerrainType)Rng::NextU32(1, (U32)TerrainType::Max);
			mapTile->unit        = entry->Handle();
			//mapTile->terrainFeaturesLen = Rng::NextU32(64, MaxTerrainFeatures + 1);
			mapTile->terrainFeaturesLen = MaxTerrainFeatures;
			for (U32 i = 0; i < mapTile->terrainFeaturesLen; i++) {
				mapTile->terrainFeatures[i] = {
					.sprite   = grassFeatureSprites[Rng::NextU32(0, MaxGrassFeatures)],
					.hexCoord = RngHexCoord(),
				};
			}
		}
	}

	mousePos = Vec2(0.f, 0.f);
	const Vec2 canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
	mouseMapCoord = PixelToMapCoord(canvasMousePos);

	state = State::WaitingOrder;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Update(U64 ticks) {
	const F32 secs = (F32)Time::Secs(ticks);
	secs;

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
						if (!event.key.down) {
							break;
						}
						if (Event::IsKeyDown(Event::Key::CtrlLeft) || Event::IsKeyDown(Event::Key::CtrlRight)) {
							if (mouseMapCoord.col >= 0 && mouseMapCoord.col < MapCols && mouseMapCoord.row >= 0 && mouseMapCoord.row < MapRows) {
								MapTile* const mapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
								if (!mapTile->unit) {
									auto const entry = units.Alloc();
									entry->obj.unitDef = spearmenUnitDef;
									entry->obj.hp      = spearmenUnitDef->maxHp;
									entry->obj.mapTile = mapTile;
									mapTile->unit = entry->Handle();
								}
							}
						} else {
							if (state == State::WaitingOrder) {
								if (mouseMapCoord.col >= 0 && mouseMapCoord.col < MapCols && mouseMapCoord.row >= 0 && mouseMapCoord.row < MapRows) {
									MapTile* const mapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
									if (mapTile->unit) {
										selectedMapTile = mapTile;
										Logf("Selected unit (%i, %i)", mapTile->mapCoord.col, mapTile->mapCoord.row);
									}
								}
							}
						}
						break;

					case Event::Key::MouseRight:
						//if (state == State::WaitingOrder && selectedMapTile
						break;
				}
				break;

			case Event::Type::MouseMove:
				mousePos.x = (F32)event.mouseMove.x;
				mousePos.y = (F32)event.mouseMove.y;

				const Vec2 canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
				mouseMapCoord = PixelToMapCoord(canvasMousePos);
				if (mouseMapCoord.col >= 0 && mouseMapCoord.col < MapCols && mouseMapCoord.row >= 0 && mouseMapCoord.row < MapRows) {
					hoverMapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
				} else {
					hoverMapTile = nullptr;
				}

				break;
		}
	}

	if (Event::IsKeyDown(Event::Key::Down)) { unitScale *= 0.9995f; }
	if (Event::IsKeyDown(Event::Key::Up)) { unitScale*= 1.0005f; }

	if (Event::IsKeyDown(Event::Key::Z)) { canvasScale *= 0.9995f; }
	if (Event::IsKeyDown(Event::Key::X)) { canvasScale *= 1.0005f; }

	const Vec2 canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
	mouseMapCoord = PixelToMapCoord(canvasMousePos);
	if (mouseMapCoord.col >= 0 && mouseMapCoord.col < MapCols && mouseMapCoord.row >= 0 && mouseMapCoord.row < MapRows) {
		hoverMapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
	} else {
		hoverMapTile = nullptr;
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw(Gpu::Frame const* gpuFrame) {
	Draw::BeginFrame(gpuFrame);

	Draw::SetCanvas(canvas);
	Draw::Draw({
		.pos   = Vec2(canvasSize.x / 2.f, canvasSize.y / 2.f),
		.size  = canvasSize,
		.color = MapBackgroundColor,
	});

	for (I32 col = 0; col < MapCols; col++) {
		for (I32 row = 0; row < MapRows; row++) {
			MapTile const* const mapTile = &mapTiles[col + row * MapCols];
			Vec2 const centerPos = MapCoordToCenterPixel(mapTile->mapCoord);
			Draw::Draw({
				.sprite = hexSprite,
				.pos    = centerPos,
				.color  = MapBackgroundColor,
			});
			for (U32 i = 0; i < mapTile->terrainFeaturesLen; i++) {
				const Vec2 featurePos(
					centerPos.x + (F32)(mapTile->terrainFeatures[i].hexCoord.x - (HexSize / 2)),
					centerPos.y + (F32)(mapTile->terrainFeatures[i].hexCoord.y - (HexSize / 2))
				);
				Draw::Draw({
					.sprite = mapTile->terrainFeatures[i].sprite,
					.pos   =  featurePos,
				});
			}

			if (mapTile->unit) {
				UnitObj const* const unitObj = units.Get(mapTile->unit);

				F32 outlineWidth = 0.f;
				if (mapTile == selectedMapTile) {
					outlineWidth = 1.f;
				}
				Draw::Draw({
					.sprite       = unitObj->unitDef->sprite,
					.pos          = centerPos,
					.size         = Vec2(unitScale, unitScale),
					.outlineColor = SelectedColor,
					.outlineWidth = outlineWidth,
				});
			}

			if (mapTile == selectedMapTile) {
				Draw::Draw({
					.sprite = hexBorderSprite,
					.pos    = centerPos,
					.color  = SelectedColor
				});
			}
		}
	}
	
	if (hoverMapTile && hoverMapTile != selectedMapTile) {
		Draw::Draw({
			.sprite = hexBorderSprite,
			.pos    = MapCoordToCenterPixel(mouseMapCoord),
			.color  = HoverColor
		});
	}

	Draw::SetDefaultCanvas();
	const Vec2 canvasCenter(canvasTopLeft.x + (canvasSize.x * canvasScale) / 2.f, canvasTopLeft.y + (canvasSize.y * canvasScale) / 2.f);
	Draw::Draw({
		.canvas = canvas,
		.pos    = canvasCenter,
		.size   = Vec2(canvasScale, canvasScale),
	});

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