#include "JC/Battle_Internal.h"

#include "JC/Draw.h"
#include "JC/Log.h"
#include "JC/Unit.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr F32 CameraSpeedPixelsPerSec = 1000.f;

static constexpr F32 Z_Hex = 0.f;

static Mem          tempMem;
static Vec2         windowSize;
static Draw::Sprite borderSprite;
static Draw::Sprite highlightSprite;
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
	TryTo(Draw::GetSprite(battleJson->borderSprite),    borderSprite);
	TryTo(Draw::GetSprite(battleJson->highlightSprite), highlightSprite);
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
	data->cameraPos.x += dx * CameraSpeedPixelsPerSec * sec;
	data->cameraPos.y += dy * CameraSpeedPixelsPerSec * sec;
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

void Draw(Data const* data) {
	Draw::SetCamera({ .pos = data->cameraPos, .scale = data->cameraScale });

	for (U32 i = 0; i < data->hexesLen; i++) {
		Hex const* const hex = &data->hexes[i];
		Vec2 const topLeftPos = HexToTopLeftWorldPos(hex);
		Draw::DrawSprite({
			.sprite = hex->terrain->sprite,
			.pos    = topLeftPos,
			.z      = Z_Hex,
			.origin = Draw::Origin::TopLeft,
		});/*
		Draw::DrawStr({
			.font   = numberFont,
			.str    = SPrintf(tempMem, "%u", hex->terrain->moveCost),
			.pos    = Vec2(topLeftPos.x + (F32)HexSize / 2.f, topLeftPos.y + 3.f),
			.z      = Z_Hex + 10.f,
			.origin = Draw::Origin::TopCenter,
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
		});*/

		if (!hex->unitData) { continue; }

		Unit::Data const* const unitData = hex->unitData;
		Draw::DrawSprite({
			.sprite = unitData->defData->sprite,
			.pos    = unitData->pos,
			.z      = Z_Hex + 1.f,
			.origin = Draw::Origin::Center,
			.flip   = unitData->side == Unit::Side::Right,
		});
	}

	if (data->hoverHex) {
		Draw::DrawSprite({
			.sprite = borderSprite,
			.pos    = HexToTopLeftWorldPos(data->hoverHex),
			.z      = Z_Hex + 0.1f,
			.origin = Draw::Origin::TopLeft,
			.color  = Vec4(1.f, 1.f, 1.f, 0.5f),
		});
	}

	if (!data->selectedHex) { return; }

	Draw::DrawSprite({
		.sprite = borderSprite,
		.pos    = HexToTopLeftWorldPos(data->selectedHex),
		.z      = Z_Hex + 0.2f,
		.origin = Draw::Origin::TopLeft,
		.color  = Vec4(1.f, 0.f, 0.f, 1.f),
	});

	for (U32 c = 0; c < MaxCols; c++) {
		for (U32 r = 0; r < MaxRows; r++) {
			U32 const idx = c + (r * MaxCols);
			Vec2 const topLeftPos = ColRowToTopLeftWorldPos(c, r);
			if (!data->selectedHexMoveCostMap.moveCosts[idx]) {
				Draw::DrawSprite({
					.sprite = highlightSprite,
					.pos    = ColRowToTopLeftWorldPos(c, r),
					.z      = Z_Hex + 0.1f,
					.origin = Draw::Origin::TopLeft,
					.color  = Vec4(0.f, 0.f, 0.f, 0.5f),
				});
			}/*
			Draw::DrawStr({
				.font   = numberFont,
				.str    = SPrintf(tempMem, "%u", data->selectedHexMoveCostMap.moveCosts[idx]),
				.pos    = Vec2(topLeftPos.x + (F32)HexSize / 2.f, topLeftPos.y + (F32)HexSize - 3.f),
				.z      = Z_Hex + 10.f,
				.origin = Draw::Origin::BottomCenter,
				.color  = Vec4(1.f, 1.f, 1.f, 1.f),
			});*/
		}
	}

	for (U32 i = 0; i < data->selectedHexToHoverHexPath.len; i++) {
		Vec2 const topLeftPos = HexToTopLeftWorldPos(data->selectedHexToHoverHexPath.hexes[i]);
		Draw::DrawSprite({
			.sprite = highlightSprite,
			.pos    = topLeftPos,
			.z      = Z_Hex + 0.1f,
			.origin = Draw::Origin::TopLeft,
			.color  = Vec4(0.f, 1.f, 0.f, 0.5f),
		});
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle