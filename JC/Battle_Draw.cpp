#include "JC/Battle_Internal.h"

#include "JC/Draw.h"
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
static U32          uiFontLineHeight;
static Draw::Font   fancyFont;
static Draw::Camera camera;

//--------------------------------------------------------------------------------------------------

Res<> InitDraw(Mem tempMemIn, Window::State const* windowState) {
	tempMem = tempMemIn;

	windowSize = Vec2((F32)windowState->width, (F32)windowState->height);

	TryTo(Draw::LoadFont("Assets/Font_EverydayStandard6.json"), numberFont);
	TryTo(Draw::LoadFont("Assets/Font_CelticTime15.json"), uiFont);
	TryTo(Draw::LoadFont("Assets/Font_OldeTome21.json"), fancyFont);
	uiFontLineHeight = Draw::GetFontLineHeight(uiFont);

	camera.pos = { 0.f, 0.f };
	camera.scale = 3.f;
}

//--------------------------------------------------------------------------------------------------

void UpdateCamera(F32 sec, F32 dx, F32 dy) {
}

//--------------------------------------------------------------------------------------------------

void Draw(Data const* data) {
	Draw::SetCamera(camera);

	for (U32 i = 0; i < data->hexesLen; i++) {
		Hex const* const hex = &data->hexes[i];
		Vec2 const topLeftPos = HexToTopLeftWorldPos(hex);
		Draw::DrawSprite({
			.sprite = hex->terrain->sprite,
			.pos    = topLeftPos,
			.z      = Z_Hex,
			.origin = Draw::Origin::TopLeft,
		});
		Draw::DrawStr({
			.font   = numberFont,
			.str    = SPrintf(tempMem, "%u", hex->terrain->moveCost),
			.pos    = Vec2(topLeftPos.x + (F32)HexSize / 2.f, topLeftPos.y + 3.f),
			.z      = Z_Hex + 10.f,
			.origin = Draw::Origin::TopCenter,
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
		});

		if (!hex->unitData) { continue; }

		Unit::Data const* const unitData = hex->unitData;
		Draw::DrawSprite({
			.sprite       = unitData->defData->sprite,
			.pos          = unitData->pos,
			.z            = Z_Hex + 1.f,
			.origin       = Draw::Origin::Center,
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
			}
			Draw::DrawStr({
				.font   = numberFont,
				.str    = SPrintf(tempMem, "%u", data->selectedHexMoveCostMap.moveCosts[idx]),
				.pos    = Vec2(topLeftPos.x + (F32)HexSize / 2.f, topLeftPos.y + (F32)HexSize - 3.f),
				.z      = Z_Hex + 10.f,
				.origin = Draw::Origin::BottomCenter,
				.color  = Vec4(1.f, 1.f, 1.f, 1.f),
			});
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