#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Config.h"
#include "JC/Draw.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Random.h"
#include "JC/Window.h"
#include <math.h>

namespace JC::AppShmup {

//--------------------------------------------------------------------------------------------------

static constexpr U32  WindowWidth  = 1920;
static constexpr U32  WindowHeight = 1080;
static constexpr U32  MapPadding   = 16;
static constexpr U32  MapRows      = 10;
static constexpr U32  MapCols      = 10;
static constexpr U32  CanvasWidth  = (MapPadding * 2) + 32 + (MapCols * 64);
static constexpr U32  CanvasHeight = (MapPadding * 2) + 16 + (MapRows * 48);
static constexpr Vec2 CanvasPos    = Vec2(80.f, 60.f);
static constexpr F32  CanvasScale  = 2.0f;
static constexpr U32  MaxTiles     = 32 * 32;

namespace TerrainType {
	using Enum = U32;
	constexpr U32 Grass           = 0;
	constexpr U32 Forest          = 1;
	constexpr U32 Mountain        = 2;
	constexpr U32 Hill            = 3;
	constexpr U32 Swamp           = 4;
	constexpr U32 Max             = 5;
};

struct Terrain {
	TerrainType::Enum type;
	Draw::Sprite      sprite;
	Draw::Sprite      spriteBordered;
};

struct Tile {
	Terrain* terrain;
	U32      row;
	U32      col;
};

static Mem::Allocator*      allocator; 
static Mem::TempAllocator*  tempAllocator; 
static Log::Logger*         logger; 
static F32                  windowWidth;
static F32                  windowHeight;
static Bool                 keyDown[(U32)Event::Key::Max];
static I32                  mouseX;
static I32                  mouseY;
static Draw::Canvas         canvas;
static Terrain              terrain[TerrainType::Max];
static Tile                 tiles[MaxTiles];
static U32                  tilesLen;
static bool                 drawBordered;
static U32                  terrainHoverCol;
static U32                  terrainHoverRow;
static Draw::Sprite         terrainHighlightSprite;


//---------------------------------------------------------------------------------------------

static Res<> PreInit(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
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

static U8 hexLut[32 * 24];

static void MakeHexLut() {
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
	JC_CHECK_RES(Gpu::ImmediateWait());

	terrain[TerrainType::Grass].type    = TerrainType::Grass;
	terrain[TerrainType::Forest].type   = TerrainType::Forest;
	terrain[TerrainType::Mountain].type = TerrainType::Mountain;
	terrain[TerrainType::Hill].type     = TerrainType::Hill;
	terrain[TerrainType::Swamp].type    = TerrainType::Swamp;
	JC_CHECK_RES(Draw::GetSprite("Terrain_Grass"            ).To(terrain[TerrainType::Grass   ].sprite));
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

	MakeHexLut();

	return Ok();
}

//---------------------------------------------------------------------------------------------

static void Shutdown() {
	Draw::DestroyCanvas(canvas);
	Draw::Shutdown();
}

//---------------------------------------------------------------------------------------------

static Res<> Events(Span<Event::Event> events) {
	for (U64 i = 0; i < events.len; i++) {
		Event::Event* ev = &events[i];
		switch (ev->type) {
			case Event::Type::Exit: {
				App::Exit();
				break;
			}

			case Event::Type::WindowResized: {
				windowWidth  = (F32)ev->windowResized.width;
				windowHeight = (F32)ev->windowResized.height;
				break;
			}

			case Event::Type::Key: {
				switch (ev->key.key) {
					case Event::Key::Escape: App::Exit(); break;
					case Event::Key::B: if (!ev->key.down) { drawBordered = !drawBordered; } break;
				}
				keyDown[(U32)events[i].key.key] = events[i].key.down;
				break;
			}

			case Event::Type::MouseMove: {
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
		JC_LOG("({}, {}) -> ({}, {}) -> out of bounds", ix, iy, col, row);
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
		JC_LOG("({}, {}) -> ({}, {}) -> out of bounds", ix, iy, col, row);
		terrainHoverCol = terrainHoverRow = U32Max;
		return;
	}
	terrainHoverCol = col;
	terrainHoverRow = row;
	JC_LOG("({}, {}) -> ({}, {}) -> ({}, {}) = {} -> ({}, {})", ix, iy, col, row, lx, ly, l, terrainHoverCol, terrainHoverRow);
}

//---------------------------------------------------------------------------------------------

static Res<> Update(double secs) {
	secs;
	//const F32 fsecs = (F32)secs;
	UpdateTerrainHover();

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

	if (terrainHoverCol != U32Max && terrainHoverRow != U32Max) {
		const I32 parity = (I32)(terrainHoverRow & 1);
		I32 px = (I32)MapPadding;
		px += (parity * 32);
		px += (terrainHoverCol * 64);
		const Vec2 pos = Vec2(
			(F32)((I32)MapPadding + ((terrainHoverRow & 1) * 32) + (terrainHoverCol * 64)),
			//(F32)px,
			(F32)((I32)MapPadding + (terrainHoverRow * 48))
		);
		//JC_LOG("px={}, {},{} -> parity {} -> pos ({.2}, {.2})", px, terrainHoverCol, terrainHoverRow, parity, pos.x, pos.y);

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

App::Fns appFns = {
	.PreInit  = PreInit,
	.Init     = Init,
	.Shutdown = Shutdown,
	.Events   = Events,
	.Update   = Update,
	.Draw     = Draw,
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC::AppShmup