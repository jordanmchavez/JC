#include "JC/Battle_Internal.h"

#include "JC/Draw.h"
#include "JC/Log.h"
#include "JC/Unit.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr F32 CameraSpeedPixelsPerSec = 1000.f;

static constexpr F32 Z_Background   = 0.0f;
static constexpr F32 Z_Hex          = 1.0f;
static constexpr F32 Z_ReachableHex = 1.1f;
static constexpr F32 Z_SelectedHex  = 1.2f;
static constexpr F32 Z_HoverHex     = 1.3f;
static constexpr F32 Z_Path         = 1.4f;
static constexpr F32 Z_HexUi        = 1.5f;
static constexpr F32 Z_Unit         = 2.0f;
static constexpr F32 Z_UiBackground = 3.0f;
static constexpr F32 Z_Ui           = 3.1f;

static constexpr F32  UiWidth           = 400.f;
static constexpr Vec4 UiBackgroundColor = Vec4(0.2f, 0.3f, 0.4f, 1.f);
static constexpr F32  UiFontScale       = 2.f;

static Mem          tempMem;
static Vec2         windowSize;
static Draw::Sprite borderSprite;
static Draw::Sprite highlightSprite;
static Draw::Sprite pathTopLeftSprite;
static Draw::Sprite pathTopRightSprite;
static Draw::Sprite pathRightSprite;
static Draw::Sprite pathBottomRightSprite;
static Draw::Sprite pathBottomLeftSprite;
static Draw::Sprite pathLeftSprite;
static Draw::Font   numberFont;
static Draw::Font   uiFont;
static F32          uiFontLineHeight;
static Draw::Font   fancyFont;

//--------------------------------------------------------------------------------------------------

Res<> InitDraw(Data* data, Mem tempMemIn, Window::State const* windowState) {

	tempMem = tempMemIn;

	windowSize = Vec2((F32)windowState->width, (F32)windowState->height);

	TryTo(Draw::LoadFont("Assets/Font_EverydayStandard6.json5"), numberFont);
	TryTo(Draw::LoadFont("Assets/Font_CelticTime15.json5"), uiFont);
	TryTo(Draw::LoadFont("Assets/Font_OldeTome21.json5"), fancyFont);
	uiFontLineHeight = Draw::GetFontLineHeight(uiFont);

	data->cameraPos = { 0.f, 0.f };
	data->cameraScale = 3.f;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> LoadDraw(BattleJson const* battleJson) {
	TryTo(Draw::GetSprite(battleJson->borderSprite),          borderSprite);
	TryTo(Draw::GetSprite(battleJson->highlightSprite),       highlightSprite);
	TryTo(Draw::GetSprite(battleJson->pathBottomLeftSprite),  pathBottomLeftSprite);
	TryTo(Draw::GetSprite(battleJson->pathLeftSprite),        pathLeftSprite);
	TryTo(Draw::GetSprite(battleJson->pathTopLeftSprite),     pathTopLeftSprite);
	TryTo(Draw::GetSprite(battleJson->pathTopRightSprite),    pathTopRightSprite);
	TryTo(Draw::GetSprite(battleJson->pathRightSprite),       pathRightSprite);
	TryTo(Draw::GetSprite(battleJson->pathBottomRightSprite), pathBottomRightSprite);
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Hex const* ScreenPosToHex(Data const* data, I32 x, I32 y) {
	return WorldPosToHex(data, Vec2(
		((F32)x / data->cameraScale) + data->cameraPos.x,
		((F32)y / data->cameraScale) + data->cameraPos.y
	));
}

//--------------------------------------------------------------------------------------------------

void MoveCamera(Data* data, F32 sec, F32 dx, F32 dy) {
	data->cameraPos.x += dx * CameraSpeedPixelsPerSec * sec / data->cameraScale;
	data->cameraPos.y += dy * CameraSpeedPixelsPerSec * sec / data->cameraScale;
}

//--------------------------------------------------------------------------------------------------

void ZoomCamera(Data* data, F32 d) {
	F32 const oldScale = data->cameraScale;
	if (data->cameraScale + d >= 1.f) {
		data->cameraScale += d;
	}
	F32 const windowCenterX = windowSize.x * 0.5f;
	F32 const windowCenterY = windowSize.y * 0.5f;
	data->cameraPos.x -= windowCenterX / data->cameraScale - windowCenterX / oldScale;
	data->cameraPos.y -= windowCenterY / data->cameraScale - windowCenterY / oldScale;
	Logf("camera scale = %f", data->cameraScale);
}

//--------------------------------------------------------------------------------------------------

static void DrawHexesAndUnits(Data const* data) {
	for (U32 i = 0; i < data->hexesLen; i++) {
		Hex const* const hex = &data->hexes[i];
		Vec2 const topLeftPos = HexToTopLeftWorldPos(hex);
		Draw::DrawSprite({
			.sprite = hex->terrain->sprite,
			.pos    = topLeftPos,
			.z      = Z_Hex,
			.origin = Draw::Origin::TopLeft,
		});
/*
		Draw::DrawStr({
			.font   = numberFont,
			.str    = SPrintf(tempMem, "%u", hex->terrain->moveCost),
			.pos    = Vec2(topLeftPos.x + (F32)HexSize / 2.f, topLeftPos.y + 3.f),
			.z      = Z_HexUi,
			.origin = Draw::Origin::TopCenter,
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
		});
*/

		if (!hex->unit) { continue; }

		Unit const* const unit = hex->unit;
		Draw::DrawSprite({
			.sprite = unit->def->sprite,
			.pos    = unit->pos,
			.z      = Z_Unit,
			.origin = Draw::Origin::Center,
			.flip   = unit->side == Side::Right,
		});

		if (data->selectedHexAttackable[i]) {
			Draw::DrawSprite({
				.sprite = borderSprite,
				.pos    = topLeftPos,
				.z      = Z_SelectedHex,
				.origin = Draw::Origin::TopLeft,
				.color  = Vec4(1.f, 0.f, 0.f, 1.f),
			});
		}
	}
}

static constexpr Vec4 SelectedHexColor  = Vec4(0.f, 1.f, 0.f, 1.f);
static constexpr Vec4 HoverHexColor     = Vec4(0.f, 1.f, 0.f, 1.f);
static constexpr Vec4 ReachableHexColor = Vec4(1.f, 1.f, 1.f, 0.25f);
static constexpr Vec4 PathColor         = Vec4(1.f, 1.f, 0.f, 1.f);

static void DrawHexDecorations(Data const* data) {
	if (data->selectedHex) {
		Draw::DrawSprite({
			.sprite = borderSprite,
			.pos    = HexToTopLeftWorldPos(data->selectedHex),
			.z      = Z_SelectedHex,
			.origin = Draw::Origin::TopLeft,
			.color  = SelectedHexColor,
		});
	}

	if (data->hoverHex) {
		Draw::DrawSprite({
			.sprite = borderSprite,
			.pos    = HexToTopLeftWorldPos(data->hoverHex),
			.z      = Z_HoverHex,
			.origin = Draw::Origin::TopLeft,
			.color  = HoverHexColor,
		});
	}

	if (data->selectedHex) {
		for (U32 r = 0; r < MaxRows; r++) {
			for (U32 c = 0; c < MaxCols; c++) {
				U32 const idx = c + (r * MaxCols);
				Vec2 const topLeftPos = ColRowToTopLeftWorldPos(c, r);
	
				if (data->selectedHexPathMap.moveCosts[idx] != U32Max && !data->hexes[idx].unit) {
					Draw::DrawSprite({
						.sprite = highlightSprite,
						.pos    = ColRowToTopLeftWorldPos(c, r),
						.z      = Z_ReachableHex,
						.origin = Draw::Origin::TopLeft,
						.color  = ReachableHexColor,
					});
				}
	/*
				if (!data->selectedHexPathMap.moveCosts[idx]) {
					Draw::DrawSprite({
						.sprite = highlightSprite,
						.pos    = ColRowToTopLeftWorldPos(c, r),
						.z      = Z_ReachableHex,
						.origin = Draw::Origin::TopLeft,
						.color  = Vec4(0.f, 0.f, 0.f, 0.5f),
					});
				}
				*/
/*
				Draw::DrawStr({
					.font   = numberFont,
					.str    = SPrintf(tempMem, "%u", data->selectedHexPathMap.moveCosts[idx]),
					.pos    = Vec2(topLeftPos.x + (F32)HexSize / 2.f, topLeftPos.y + (F32)HexSize - 3.f),
					.z      = Z_HexUi,
					.origin = Draw::Origin::BottomCenter,
					.color  = Vec4(1.f, 1.f, 1.f, 1.f),
				});
*/
			}
		}

		auto SelectPathSprite = [](Hex const* fromHex, Hex const* toHex) {
			if (toHex == fromHex->neighbors[NeighborIdx_TopLeft    ]) { return pathTopLeftSprite; }
			if (toHex == fromHex->neighbors[NeighborIdx_TopRight   ]) { return pathTopRightSprite; }
			if (toHex == fromHex->neighbors[NeighborIdx_Right      ]) { return pathRightSprite; }
			if (toHex == fromHex->neighbors[NeighborIdx_BottomRight]) { return pathBottomRightSprite; }
			if (toHex == fromHex->neighbors[NeighborIdx_BottomLeft ]) { return pathBottomLeftSprite; }
			if (toHex == fromHex->neighbors[NeighborIdx_Left       ]) { return pathLeftSprite; }
			Panic("Hexes (%i, %i) and (%i, %i) are not adjacent!", fromHex->c, fromHex->r, toHex->c, toHex->r);
		};

		Hex const* fromHex = data->selectedHex;
		for (U32 i = 0; i < data->selectedHexToHoverHexPath.len; i++) {
			Hex const* const toHex = data->selectedHexToHoverHexPath.hexes[i];
			Draw::DrawSprite({
				.sprite = SelectPathSprite(fromHex, toHex),
				.pos    = HexToTopLeftWorldPos(fromHex),
				.z      = Z_Path,
				.origin = Draw::Origin::TopLeft,
				.color  = PathColor,
			});
			Draw::DrawSprite({
				.sprite = SelectPathSprite(toHex, fromHex),
				.pos    = HexToTopLeftWorldPos(toHex),
				.z      = Z_Path,
				.origin = Draw::Origin::TopLeft,
				.color  = PathColor,
			});

			fromHex = toHex;
		}
		if (
			data->hoverHex &&
			data->hoverHex->unit &&
			data->hoverHex->unit->side != data->selectedHex->unit->side
		) {
			if (AreHexesAdjacent(fromHex, data->hoverHex)) {
				Draw::DrawSprite({
					.sprite = SelectPathSprite(fromHex, data->hoverHex),
					.pos    = HexToTopLeftWorldPos(fromHex),
					.z      = Z_Path,
					.origin = Draw::Origin::TopLeft,
					.color  = PathColor,
				});
				Draw::DrawSprite({
					.sprite = SelectPathSprite(data->hoverHex, fromHex),
					.pos    = HexToTopLeftWorldPos(data->hoverHex),
					.z      = Z_Path,
					.origin = Draw::Origin::TopLeft,
					.color  = Vec4(1.f, 0.f, 0.f, 1.f),
				});
			}
		}
	}
}

static void DrawUi(Data const* data) {
	Draw::DrawRect({
		.pos    = windowSize,
		.z      = Z_UiBackground,
		.origin = Draw::Origin::BottomRight,
		.size   = Vec2(UiWidth, windowSize.y),
		.color  = UiBackgroundColor,
	});

	Draw::StrDrawDef drawDef = {
		.font   = uiFont,
		.pos    = { windowSize.x - UiWidth + 10.f, 10.f },
		.z      = Z_Ui,
		.origin = Draw::Origin::TopLeft,
		.scale  = { UiFontScale, UiFontScale },
		.color  = { 1.f, 1.f, 1.f, 1.f },
	};

	if (data->hoverHex) {
		Terrain const* const terrain = data->hoverHex->terrain;
		drawDef.str = SPrintf(tempMem, "%s: %u move", terrain->name, terrain->moveCost);
		Draw::DrawStr(drawDef);
		drawDef.pos.y += (uiFontLineHeight + 3) * UiFontScale;
	}

	if (data->selectedHex) {
		Unit const* const unit = data->selectedHex->unit;
		JC::Unit::Def const* const def = unit->def;

		drawDef.str    = def->name;
		Draw::DrawStr(drawDef);
		drawDef.pos.y += (uiFontLineHeight + 3) * UiFontScale;

		drawDef.str = SPrintf(tempMem, "HP: %u/%u", unit->hp, def->hp);
		Draw::DrawStr(drawDef);
		drawDef.pos.y += (uiFontLineHeight + 3) * UiFontScale;

		drawDef.str = SPrintf(tempMem, "Move: %u/%u", unit->move, def->move);
		Draw::DrawStr(drawDef);
		drawDef.pos.y += (uiFontLineHeight + 3) * UiFontScale;
	}
}

void Draw(Data const* data) {
	Draw::SetCamera({ .pos = data->cameraPos, .scale = data->cameraScale });
	DrawHexesAndUnits(data);
	DrawHexDecorations(data);
	Draw::ClearCamera();
	DrawUi(data);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle