#include "JC/Array.h"
#include "JC/Config.h"
#include "JC/Draw.h"
#include "JC/Fmt.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Random.h"
#include "JC/Time.h"
#include "JC/Window.h"
#include <math.h>

namespace JC::Game {

//--------------------------------------------------------------------------------------------------

static constexpr U32  WindowWidth  = 1920;
static constexpr U32  WindowHeight = 1080;
static constexpr U32  CanvasWidth  = 640;
static constexpr U32  CanvasHeight = 480;
static constexpr Vec2 CanvasPos    = Vec2(80.f, 60.f);
static constexpr F32  CanvasScale  = 1.5f;

static Allocator*     allocator; 
static TempAllocator* tempAllocator; 
static Log::Logger*   logger; 
static F32            windowWidth;
static F32            windowHeight;
static I32            mouseX;
static I32            mouseY;
static Draw::Canvas   canvas;

//---------------------------------------------------------------------------------------------

static Res<> PreInit(Allocator* allocatorIn, TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
	allocator     = allocatorIn;
	tempAllocator = tempAllocatorIn;
	logger        = loggerIn;
	Config::SetStr(App::Cfg_Title,        "Tbs");
	Config::SetU32(App::Cfg_WindowStyle,  (U32)Window::Style::BorderedResizable);
	Config::SetU32(App::Cfg_WindowWidth,  WindowWidth);
	Config::SetU32(App::Cfg_WindowHeight, WindowHeight);
	return Ok();
}

//---------------------------------------------------------------------------------------------

static Res<> Init(const Window::State* windowState) {
	windowWidth  = (F32)windowState->width;
	windowHeight = (F32)windowState->height;

	Draw::InitDesc initDesc = {
		.allocator     = allocator,
		.tempAllocator = tempAllocator,
		.logger        = logger,
		.windowWidth   = WindowWidth,
		.windowHeight  = WindowHeight,

	};
	JC_CHECK_RES(Draw::Init(&initDesc));

	JC_CHECK_RES(Draw::CreateCanvas(CanvasWidth, CanvasHeight).To(canvas));

	JC_CHECK_RES(Draw::LoadSpriteAtlas("Assets/Terrain.png", "Assets/Terrain.atlas"));
	JC_CHECK_RES(Draw::LoadSpriteAtlas("Assets/Units.png",   "Assets/Units.atlas"));
	JC_CHECK_RES(Gpu::ImmediateWait());

	terrain[TerrainType::Grass].type    = TerrainType::Grass;
	terrain[TerrainType::Forest].type   = TerrainType::Forest;
	terrain[TerrainType::Mountain].type = TerrainType::Mountain;
	terrain[TerrainType::Hill].type     = TerrainType::Hill;
	terrain[TerrainType::Swamp].type    = TerrainType::Swamp;
	auto foo = Draw::GetSprite("Terrain_Grass"            ).To(terrain[TerrainType::Grass   ].sprite);
	JC_CHECK_RES(foo);
	JC_CHECK_RES(Draw::GetSprite("Terrain_Grass_Bordered"   ).To(terrain[TerrainType::Grass   ].spriteBordered));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Forest"           ).To(terrain[TerrainType::Forest  ].sprite));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Forest_Bordered"  ).To(terrain[TerrainType::Forest  ].spriteBordered));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Hill"             ).To(terrain[TerrainType::Hill    ].sprite));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Hill_Bordered"    ).To(terrain[TerrainType::Hill    ].spriteBordered));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Mountain"         ).To(terrain[TerrainType::Mountain].sprite));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Mountain_Bordered").To(terrain[TerrainType::Mountain].spriteBordered));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Swamp"            ).To(terrain[TerrainType::Swamp   ].sprite));
	JC_CHECK_RES(Draw::GetSprite("Terrain_Swamp_Bordered"   ).To(terrain[TerrainType::Swamp   ].spriteBordered));

	for (U32 r = 0; r < MapRows; r++) {
		for (U32 c = 0; c < MapCols; c++) {
			tiles[tilesLen++] = {
				.terrain = &terrain[Random::NextU32() % TerrainType::Max],
				.row = r,
				.col = c,
			};
		}
	}

	JC_CHECK_RES(Draw::GetSprite("Terrain_Highlight").To(terrainHighlightSprite));

	InitHexLut();
	JC_CHECK_RES(InitArmies());

	return Ok();
}

//---------------------------------------------------------------------------------------------

static void Shutdown() {
	Draw::DestroyCanvas(canvas);
	Draw::Shutdown();
}

//---------------------------------------------------------------------------------------------

static Res<> Events(Span<const Window::Event> events) {
	for (U64 i = 0; i < events.len; i++) {
		const Window::Event* ev = &events[i];
		switch (ev->type) {
			case Window::EventType::Exit: {
				App::Exit();
				break;
			}

			case Window::EventType::WindowResized: {
				windowWidth  = (F32)ev->windowResized.width;
				windowHeight = (F32)ev->windowResized.height;
				break;
			}

			case Window::EventType::Key: {
				switch (ev->key.key) {
					case Window::Key::Escape: App::Exit(); break;
					case Window::Key::B: if (!ev->key.down) { drawBordered = !drawBordered; } break;
					case Window::Key::Mouse1:
						JC_LOG("mouse left");
						break;
						
				}
				break;
			}

			case Window::EventType::MouseMove: {
				mouseX = ev->mouseMove.x;
				mouseY = ev->mouseMove.y;
				break;
			}

			
		}
	}
	return Ok();
}

//---------------------------------------------------------------------------------------------

static void UpdateTerrainHover() {
	Vec2 pos = { (F32)mouseX, (F32)mouseY };
	pos = Math::Sub(pos, CanvasPos);
	pos = Math::Scale(pos, 1.f / CanvasScale);
	pos = Math::Sub(pos, Vec2(MapPadding, MapPadding));


	I32 iy = (I32)pos.y;
	I32 row = iy / 48;
	const U8 parity = (row & 1);

	I32 ix = (I32)pos.x - parity * 32;
	I32 col = ix / 64;
	if (ix < 0 || iy < 0) {
		terrainHoverCol = terrainHoverRow = U32Max;
		return;
	}

	I32 lx = ix - (col * 64);
	I32 ly = iy - (row * 48);
	JC_ASSERT(lx >= 0 && ly >= 0);

	const U8 l = (hexLut[(ly / 2) * 32 + (lx / 2)] >> ((lx & 1) * 4)) & 0xf;

	switch (l) {
		case 1: col -= 1 * (1 - parity); row -= 1; break;
		case 2: col += 1 * parity;       row -= 1; break;
	}
	if (col >= MapCols || row >= MapRows) {
		terrainHoverCol = terrainHoverRow = U32Max;
		return;
	}
	terrainHoverCol = col;
	terrainHoverRow = row;
}

//---------------------------------------------------------------------------------------------

static Res<> Update(U64 ticks) {
	return Ok();
}

//---------------------------------------------------------------------------------------------

static Res<> Draw(Gpu::Frame frame) {
	Draw::BeginFrame(frame);

	Draw::SetCanvas(canvas);

	Draw::DrawRect(Vec2(0.f, 0.f), Vec2(CanvasWidth, CanvasHeight), Vec4(0.15f, 0.3f, 0.25f, 1.0f), Vec4(1.f, 1.f, 1.f, 1.f), 0.f, 0.f);
	for (U64 i = 0; i < tilesLen; i++) {
		const Tile* const tile = &tiles[i];
		Draw::DrawSprite(
			drawBordered ? tile->terrain->spriteBordered : tile->terrain->sprite,
			Vec2(
				(F32)(MapPadding + ((tile->row & 1) * 32) + (tile->col * 64)),
				(F32)(MapPadding + (tile->row * 48))
			)
		);
	}

	/*
	const U32 unitRowParity = spearmanRow & 1;
	constexpr Vec2 UnitSize = { 16.f, 16.f };
	const F32 unitX = (F32)(MapPadding + (spearmanCol * 64) + (unitRowParity * 32) + 32) - (UnitSize.x / 2);
	const F32 unitY = (F32)(MapPadding + (spearmanRow * 48)                        + 32) - (UnitSize.y / 2);
	Draw::DrawSprite(spearmanSprite, Vec2(unitX, unitY));
	*/
	if (terrainHoverCol != U32Max && terrainHoverRow != U32Max) {
		const Vec2 pos = Vec2(
			(F32)((I32)MapPadding + ((terrainHoverRow & 1) * 32) + (terrainHoverCol * 64)),
			(F32)((I32)MapPadding + (terrainHoverRow * 48))
		);
		Draw::DrawSprite(
			terrainHighlightSprite, 
			pos
		);
	}


	Draw::SetCanvas();
	Draw::DrawCanvas(canvas, CanvasPos, Vec2(CanvasScale, CanvasScale));

	Draw::EndFrame();
	return Ok();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Game