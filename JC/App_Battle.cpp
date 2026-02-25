#pragma warning(disable: 4505)

#include "JC/App.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Event.h"
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
static constexpr U32 MaxUnitDefs = 64;
static constexpr U32 MaxUnits = 256;
static constexpr Vec4 MapBackgroundColor = Vec4(13.f/255.f, 30.f/255.f, 22.f/255.f, 1.f);
static constexpr Vec4 SelectedColor = Vec4(0.f, 1.f, 0.f, 1.f);
static constexpr Vec4 HoverColor = Vec4(1.f, 1.f, 1.f, 0.75f);

constexpr F32 Z_Background   = 0.f;
constexpr F32 Z_Map          = 1.f;
constexpr F32 Z_MapHighlight = 2.f;
constexpr F32 Z_Unit         = 3.f;
constexpr F32 Z_UnitSelected = 4.f;

struct HexCoord {
	I16 x = 0;
	I16 y = 0;
};

struct MapCoord {
	I16 col = 0;
	I16 row = 0;
};

static constexpr U32 MaxSpriteAnimationFrames = 16;
struct AnimationDef {
	Draw::Sprite frameSprites[MaxSpriteAnimationFrames];
	F32          frameDurSecs[MaxSpriteAnimationFrames];
	U32          frameLen = 0;
};

struct UnitDef {
	Draw::Sprite sprite;
	Vec2         size;
	U32          maxHp = 0;
	AnimationDef attackAnimationDef;
};

struct Unit {
	bool                alive = false;
	UnitDef const*      unitDef = nullptr;
	Vec2                pos;
	F32                 z = Z_Unit;
	U32                 hp = 0;
	AnimationDef const* activeAnimationDef;
	U16                 animationFrame = 0;
	F32                 animationFrameElapsedSecs = 0.f;
};

enum struct TerrainType {
	Invalid  = 0,
	Grass,
	Forest,
	Mountain,
	Hill,
	Swamp,
	Water,
	Max,
};

struct TerrainFeature {
	Draw::Sprite sprite;
	HexCoord     hexCoord;
};

struct MapTile {
	MapCoord       mapCoord;
	TerrainType    terrainType = TerrainType::Invalid;
	Unit*          unit = nullptr;
};

enum struct State {
	WaitingOrder,
	ExecutingOrder,
};

enum struct OrderType {
	Invalid = 0,
	Move,
	Attack,
};

enum struct AttackOrderState {
	Invalid = 0,
	MovingTo,
	Attacking,
	MovingBack,
};

struct MoveOrder {
	F32      elapsedSecs = 0.f;
	F32      durSecs = 0.f;
	Unit*    unit = nullptr;
	MapTile* startMapTile = nullptr;
	MapTile* endMapTile = nullptr;
};

struct AttackOrder {
	AttackOrderState attackOrderState = AttackOrderState::Invalid;
	F32              elapsedSecs = 0.f;
	F32              durSecs = 0.f;
	MapTile*         unitMapTile = nullptr;
	Unit*            unit = nullptr;
	Unit*            targetUnit = nullptr;
	Vec2             targetPos;
};

struct Order {
	OrderType   orderType;
	MoveOrder   moveOrder;
	AttackOrder attackOrder;
};

static Mem          permMem;
static Mem          tempMem;
static U8           hexLut[(HexSize / 2) * (HexSize * 3 / 8)];
static HexCoord     hexCoordLut[(HexSize / 2) * (HexSize / 2)];
static U32          hexCoordLutLen;
static Draw::Canvas canvas;
static Vec2         canvasSize(MapCols * HexSize + (HexSize / 2), MapRows * (HexSize * 3 / 4) + (HexSize / 4));
static Vec2         canvasTopLeft(25.f, 25.f);
static MapTile      mapTiles[MapCols * MapRows];
static Vec2         mousePos;
static MapCoord     mouseMapCoord;
static UnitDef      unitDefs[MaxUnitDefs];
static U32          unitDefsLen;
static UnitDef*     spearmenUnitDef;
static Unit         units[MaxUnits];
static U32          unitsLen;
static State        state;
static Draw::Sprite hexBorderSprite;
static Draw::Sprite terrainSprites[(U32)TerrainType::Max];
static Unit*        selectedUnit;
static MapTile*     selectedMapTile;
static MapTile*     hoverMapTile;
static Order        order;

static F32 canvasScale = 2.f;

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

static HexCoord RngHexCoord() {
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

static Unit* AllocUnit() {
	Assert(unitsLen < MaxUnits);
	return &units[unitsLen++];
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
	TryTo(Draw::GetSprite("Unit_Spearmen"), spearmenUnitDef->sprite);
	Draw::Sprite attackSprite; TryTo(Draw::GetSprite("Unit_Spearmen_Attack"), attackSprite);
	spearmenUnitDef->maxHp = 10;
	spearmenUnitDef->size = Draw::GetSpriteSize(spearmenUnitDef->sprite);
	spearmenUnitDef->attackAnimationDef.frameLen = 6;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[0] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[1] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[2] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[3] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[4] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[5] = 0.2f;
	spearmenUnitDef->attackAnimationDef.frameSprites[0] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[1] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[2] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[3] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[4] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameSprites[5] = spearmenUnitDef->sprite;

	TryTo(Draw::GetSprite("Hex_Border_32"), hexBorderSprite);
	TryTo(Draw::GetSprite("Hex_Grass_32"), terrainSprites[(U32)TerrainType::Grass]);
	TryTo(Draw::GetSprite("Hex_Forest_32"), terrainSprites[(U32)TerrainType::Forest]);
	TryTo(Draw::GetSprite("Hex_Hill_32"), terrainSprites[(U32)TerrainType::Hill]);
	TryTo(Draw::GetSprite("Hex_Mountain_32"), terrainSprites[(U32)TerrainType::Mountain]);
	TryTo(Draw::GetSprite("Hex_Swamp_32"), terrainSprites[(U32)TerrainType::Swamp]);
	TryTo(Draw::GetSprite("Hex_Water_32"), terrainSprites[(U32)TerrainType::Water]);

	for (I16 c = 0; c < MapCols; c++) {
		for (I16 r = 0; r < MapRows; r++) {
			MapTile* const mapTile = &mapTiles[c + r * MapCols];
			mapTile->mapCoord    = { .col = c, .row = r };
			mapTile->terrainType = (TerrainType)Rng::NextU32(1, (U32)TerrainType::Max);
			if (Rng::NextU32(0, 100) < 50) { continue; }

			Unit* const unit = AllocUnit();
			unit->alive   = true;
			unit->unitDef = spearmenUnitDef;
			unit->pos     = MapCoordToCenterPixel(mapTile->mapCoord);
			unit->z       = Z_Unit;
			unit->hp      = spearmenUnitDef->maxHp;
			mapTile->unit = unit;
		}
	}

	mousePos = Vec2(0.f, 0.f);
	const Vec2 canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
	mouseMapCoord = PixelToMapCoord(canvasMousePos);

	state = State::WaitingOrder;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static void HandleLeftClick() {
	if (mouseMapCoord.col < 0 || mouseMapCoord.col >= MapCols || mouseMapCoord.row < 0 || mouseMapCoord.row >= MapRows) {
		return;
	}

	MapTile* const clickedMapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
	if (state == State::WaitingOrder) {
		if (!selectedUnit) {
			Assert(!selectedMapTile);
			if (clickedMapTile->unit) {
				selectedUnit = clickedMapTile->unit;
				selectedUnit->z = Z_UnitSelected;
				selectedMapTile = clickedMapTile;
				Logf("Selected unit (%i, %i)", clickedMapTile->mapCoord.col, clickedMapTile->mapCoord.row);
			} else {
				Logf("Ignoring click on empty map tile");
			}
			return;
		}

		if (selectedUnit) {
			Assert(selectedMapTile);
			if (clickedMapTile->unit == selectedUnit) {
				Logf("Deselected unit");
				selectedUnit = nullptr;
				selectedMapTile = nullptr;
				return;
			}
			if (clickedMapTile->unit) {
				Vec2 const targetUnitPos  = clickedMapTile->unit->pos;
				Vec2 const targetUnitSize = Math::Scale(clickedMapTile->unit->unitDef->size, 0.5f);

				order.orderType = OrderType::Attack;
				order.attackOrder.attackOrderState = AttackOrderState::MovingTo;
				order.attackOrder.elapsedSecs      = 0.f;
				order.attackOrder.durSecs          = 0.5f;
				order.attackOrder.unitMapTile      = selectedMapTile;
				order.attackOrder.unit             = selectedUnit;
				order.attackOrder.targetUnit       = clickedMapTile->unit;
				bool intersected = Math::IntersectLineSegmentAabb(selectedUnit->pos, targetUnitPos, Math::Sub(targetUnitPos, targetUnitSize), Math::Add(targetUnitPos, targetUnitSize), &order.attackOrder.targetPos);
				Assert(intersected);
				if (!intersected) {
					order.attackOrder.targetPos = targetUnitPos;
				}

				state = State::ExecutingOrder;

				Logf("Executing attack order");

				return;
			}

			selectedMapTile->unit = nullptr;

			order.orderType              = OrderType::Move;
			order.moveOrder.elapsedSecs  = 0.f;
			order.moveOrder.durSecs      = 0.5f;
			order.moveOrder.unit         = selectedUnit;
			order.moveOrder.startMapTile = selectedMapTile;
			order.moveOrder.endMapTile   = clickedMapTile;

			selectedMapTile = clickedMapTile;

			state = State::ExecutingOrder;

			Logf("Executing move order from (%i, %i) -> (%i, %i)", order.moveOrder.startMapTile->mapCoord.col, order.moveOrder.startMapTile->mapCoord.row, order.moveOrder.endMapTile->mapCoord.col, order.moveOrder.endMapTile->mapCoord.row);
		}
	}
}

//--------------------------------------------------------------------------------------------------

static void ExecuteMoveOrder(F32 secs) {
	MoveOrder* const moveOrder = &order.moveOrder;
	moveOrder->elapsedSecs += secs;
	if (moveOrder->elapsedSecs < moveOrder->durSecs) {
		Vec2 const startPos = MapCoordToCenterPixel(moveOrder->startMapTile->mapCoord);
		Vec2 const endPos   = MapCoordToCenterPixel(moveOrder->endMapTile->mapCoord);
		F32 const t         = moveOrder->elapsedSecs / moveOrder->durSecs;
		moveOrder->unit->pos = Math::Lerp(startPos, endPos, t);
	} else {
		moveOrder->endMapTile->unit = moveOrder->unit;
		moveOrder->unit->z = Z_Unit;
		memset(&order, 0, sizeof(order));
		selectedMapTile = nullptr;
		selectedUnit    = nullptr;
		state = State::WaitingOrder;
	}
}

static void ExecuteAttackOrder(F32 secs) {
	AttackOrder* const attackOrder = &order.attackOrder;
	Unit* const unit = attackOrder->unit;
	switch (attackOrder->attackOrderState) {
		case AttackOrderState::MovingTo: {
			attackOrder->elapsedSecs += secs;
			if (attackOrder->elapsedSecs < attackOrder->durSecs) {
				Vec2 const startPos = MapCoordToCenterPixel(attackOrder->unitMapTile->mapCoord);
				Vec2 const endPos   = attackOrder->targetPos;
				F32 const t         = attackOrder->elapsedSecs / attackOrder->durSecs;
				unit->pos = Math::Lerp(startPos, endPos, t);
			} else {
				attackOrder->attackOrderState = AttackOrderState::Attacking;
				unit->activeAnimationDef        = &unit->unitDef->attackAnimationDef;
				unit->animationFrame            = 0;
				unit->animationFrameElapsedSecs = 0.f;
			}
			break;
		}

		case AttackOrderState::Attacking:
			unit->animationFrameElapsedSecs += secs;
			if (unit->animationFrameElapsedSecs < unit->activeAnimationDef->frameDurSecs[unit->animationFrame]) {
				// nothing
			} else {
				if (unit->animationFrame < unit->activeAnimationDef->frameLen - 1) {
					unit->animationFrame++;
					unit->animationFrameElapsedSecs = 0.f;
				} else {
					unit->activeAnimationDef        = nullptr;
					unit->animationFrame            = 0;
					unit->animationFrameElapsedSecs = 0.f;
					attackOrder->attackOrderState = AttackOrderState::MovingBack;
					attackOrder->elapsedSecs     = 0.f;
					attackOrder->durSecs         = 0.5f;
				}
			}
			break;

		case AttackOrderState::MovingBack: {
			attackOrder->elapsedSecs += secs;
			if (attackOrder->elapsedSecs < attackOrder->durSecs) {
				Vec2 const startPos = attackOrder->targetPos;
				Vec2 const endPos   = MapCoordToCenterPixel(attackOrder->unitMapTile->mapCoord);
				F32 const t         = attackOrder->elapsedSecs / attackOrder->durSecs;
				unit->pos = Math::Lerp(startPos, endPos, t);
			} else {
				unit->pos = MapCoordToCenterPixel(attackOrder->unitMapTile->mapCoord);
				unit->z = Z_Unit;
				memset(&order, 0, sizeof(order));
				selectedMapTile = nullptr;
				selectedUnit    = nullptr;
				state = State::WaitingOrder;
			}
			break;
		}

		default: Panic("Unhandled AttackOrderState %u", (U32)attackOrder->attackOrderState);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> Update(U64 ticks) {
	const F32 secs = (F32)Time::Secs(ticks);

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
						HandleLeftClick();
						break;

					case Event::Key::MouseRight:
						//if (state == State::WaitingOrder && selectedMapTile
						break;
				}
				break;

			case Event::Type::MouseMove: {
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

			case Event::Type::MouseWheel:
				     if (event.mouseWheel.delta < 0.f) { canvasScale *= 0.9f; }
				else if (event.mouseWheel.delta > 0.f) { canvasScale *= 1.1f; }
				break;
		}
	}

	if (Event::IsKeyDown(Event::Key::Z)) { canvasScale *= 0.9995f; }
	if (Event::IsKeyDown(Event::Key::X)) { canvasScale *= 1.0005f; }

	const Vec2 canvasMousePos = Math::Scale(Math::Sub(mousePos, canvasTopLeft), 1.f / canvasScale);
	mouseMapCoord = PixelToMapCoord(canvasMousePos);
	if (mouseMapCoord.col >= 0 && mouseMapCoord.col < MapCols && mouseMapCoord.row >= 0 && mouseMapCoord.row < MapRows) {
		hoverMapTile = &mapTiles[mouseMapCoord.col + mouseMapCoord.row * MapCols];
	} else {
		hoverMapTile = nullptr;
	}

	if (state == State::ExecutingOrder) {
		switch (order.orderType) {
			case OrderType::Move:   ExecuteMoveOrder(secs);   break;
			case OrderType::Attack: ExecuteAttackOrder(secs); break;
			default: Panic("Unhandled OrderType %u", (U32)order.orderType);
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw(Gpu::Frame const* gpuFrame) {
	Draw::BeginFrame(gpuFrame);

	Draw::SetCanvas(canvas);
	Draw::Draw({
		.pos   = Vec2(canvasSize.x / 2.f, canvasSize.y / 2.f),
		.z     = Z_Background,
		.size  = canvasSize,
		.color = MapBackgroundColor,
	});

	for (I32 col = 0; col < MapCols; col++) {
		for (I32 row = 0; row < MapRows; row++) {
			MapTile const* const mapTile = &mapTiles[col + row * MapCols];
			Vec2 const centerPos = MapCoordToCenterPixel(mapTile->mapCoord);
			Draw::Draw({
				.sprite = terrainSprites[(U32)mapTile->terrainType],
				.pos    = centerPos,
				.z      = Z_Map,
			});
			if (mapTile == selectedMapTile) {
				Draw::Draw({
					.sprite = hexBorderSprite,
					.pos    = centerPos,
					.z      = Z_MapHighlight,
					.color  = SelectedColor
				});
			}
		}
	}
	if (hoverMapTile && hoverMapTile != selectedMapTile) {
		Draw::Draw({
			.sprite = hexBorderSprite,
			.pos    = MapCoordToCenterPixel(mouseMapCoord),
			.z      = Z_MapHighlight,
			.color  = HoverColor
		});
	}

	for (U32 i = 0; i < unitsLen; i++) {
		Unit const* const unit = &units[i];
		if (!unit->alive) { continue; }
			F32 outlineWidth = 0.f;
			if (unit == selectedUnit) {
				outlineWidth = 1.f;
			}
			Draw::Sprite sprite = unit->unitDef->sprite;
			if (unit->activeAnimationDef) {
				sprite = unit->activeAnimationDef->frameSprites[unit->animationFrame];
			}
			Draw::Draw({
				.sprite       = sprite,
				.pos          = unit->pos,
				.z            = unit->z,
				.outlineColor = SelectedColor,
				.outlineWidth = outlineWidth,
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