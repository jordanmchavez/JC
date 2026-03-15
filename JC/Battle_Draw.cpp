#include "JC/Battle_Internal.h"

#include "JC/Draw.h"
#include "JC/Log.h"
#include "JC/UnitDef.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr F32 CameraSpeedPixelsPerSec = 1000.f;

static constexpr F32  UiWidth           = 400.f;
static constexpr Vec4 UiBackgroundColor = Vec4(0.2f, 0.3f, 0.4f, 1.f);
static constexpr F32  UiFontScale       = 2.f;

static Mem          tempMem;
static Vec2         windowSize;
static Draw::Sprite borderSprite;
static Draw::Sprite innerSprite;
static Draw::Sprite pathTopLeftSprite;
static Draw::Sprite pathTopRightSprite;
static Draw::Sprite pathRightSprite;
static Draw::Sprite pathBottomRightSprite;
static Draw::Sprite pathBottomLeftSprite;
static Draw::Sprite pathLeftSprite;
static Draw::Sprite arrowTopLeftSprite;
static Draw::Sprite arrowTopRightSprite;
static Draw::Sprite arrowRightSprite;
static Draw::Sprite arrowBottomRightSprite;
static Draw::Sprite arrowBottomLeftSprite;
static Draw::Sprite arrowLeftSprite;
static Draw::Sprite attackSprite;
static Draw::Font   numberFont;
static Draw::Font   uiFont;
static F32          uiFontLineHeight;
static Draw::Font   fancyFont;
static Vec2         cameraPos;
static F32          cameraScale;

//--------------------------------------------------------------------------------------------------

Res<> InitDraw(Mem tempMemIn, Window::State const* windowState) {

	tempMem = tempMemIn;

	windowSize = Vec2((F32)windowState->width, (F32)windowState->height);

	TryTo(Draw::LoadFont("Assets/Font_EverydayStandard6.json5"), numberFont);
	TryTo(Draw::LoadFont("Assets/Font_CelticTime15.json5"), uiFont);
	TryTo(Draw::LoadFont("Assets/Font_OldeTome21.json5"), fancyFont);
	uiFontLineHeight = Draw::GetFontLineHeight(uiFont);

	cameraPos = { 0.f, 0.f };
	cameraScale = 3.f;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> LoadDraw(BattleJson const* battleJson) {
	TryTo(Draw::GetSprite(battleJson->borderSprite),           borderSprite);
	TryTo(Draw::GetSprite(battleJson->innerSprite),            innerSprite);
	TryTo(Draw::GetSprite(battleJson->pathBottomLeftSprite),   pathBottomLeftSprite);
	TryTo(Draw::GetSprite(battleJson->pathLeftSprite),         pathLeftSprite);
	TryTo(Draw::GetSprite(battleJson->pathTopLeftSprite),      pathTopLeftSprite);
	TryTo(Draw::GetSprite(battleJson->pathTopRightSprite),     pathTopRightSprite);
	TryTo(Draw::GetSprite(battleJson->pathRightSprite),        pathRightSprite);
	TryTo(Draw::GetSprite(battleJson->pathBottomRightSprite),  pathBottomRightSprite);
	TryTo(Draw::GetSprite(battleJson->arrowBottomLeftSprite),  arrowBottomLeftSprite);
	TryTo(Draw::GetSprite(battleJson->arrowLeftSprite),        arrowLeftSprite);
	TryTo(Draw::GetSprite(battleJson->arrowTopLeftSprite),     arrowTopLeftSprite);
	TryTo(Draw::GetSprite(battleJson->arrowTopRightSprite),    arrowTopRightSprite);
	TryTo(Draw::GetSprite(battleJson->arrowRightSprite),       arrowRightSprite);
	TryTo(Draw::GetSprite(battleJson->arrowBottomRightSprite), arrowBottomRightSprite);

	TryTo(Draw::GetSprite("UI_AttackBorder"), attackSprite);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Hex* ScreenPosToHex(Data* data, I32 x, I32 y) {
	return WorldPosToHex(data, Vec2(
		((F32)x / cameraScale) + cameraPos.x,
		((F32)y / cameraScale) + cameraPos.y
	));
}

//--------------------------------------------------------------------------------------------------

void MoveCamera(F32 sec, F32 dx, F32 dy) {
	cameraPos.x += dx * CameraSpeedPixelsPerSec * sec / cameraScale;
	cameraPos.y += dy * CameraSpeedPixelsPerSec * sec / cameraScale;
}

//--------------------------------------------------------------------------------------------------

void ZoomCamera(F32 d) {
	F32 const oldScale = cameraScale;
	if (cameraScale + d >= 1.f) {
		cameraScale += d;
	}
	F32 const windowCenterX = windowSize.x * 0.5f;
	F32 const windowCenterY = windowSize.y * 0.5f;
	cameraPos.x -= windowCenterX / cameraScale - windowCenterX / oldScale;
	cameraPos.y -= windowCenterY / cameraScale - windowCenterY / oldScale;
}

//--------------------------------------------------------------------------------------------------

/*

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
	Draw::DrawStr(drawDef);

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
*/

static constexpr Vec4 SelectedHexColor  = Vec4(0.f, 1.f, 0.f, 1.f);
static constexpr Vec4 ReachableHexColor = Vec4(1.f, 1.f, 1.f, 0.25f);
static constexpr Vec4 AttackableColor   = Vec4(1.f, 0.f, 0.f, 0.5f);
static constexpr Vec4 HoverPathColor    = Vec4(1.f, 1.f, 0.f, 0.5f);
static constexpr Vec4 HoverAttackColor  = Vec4(1.f, 0.f, 0.f, 0.5f);
static constexpr Vec4 TargetPathColor   = Vec4(1.f, 1.f, 0.f, 1.f);
static constexpr Vec4 TargetAttackColor = Vec4(1.f, 0.f, 0.f, 1.f);

static constexpr F32 Z_Background   = 0.00f;
static constexpr F32 Z_Hex          = 1.01f;
static constexpr F32 Z_HexFill      = 1.02f;
static constexpr F32 Z_HexBorder    = 1.03f;
static constexpr F32 Z_Path         = 1.04f;
static constexpr F32 Z_Arrow        = 1.05f;
static constexpr F32 Z_HexUi        = 1.06f;
static constexpr F32 Z_Unit         = 2.00f;
static constexpr F32 Z_UiBackground = 3.00f;
static constexpr F32 Z_Ui           = 3.01f;

//--------------------------------------------------------------------------------------------------

static void DrawInner(Vec2 pos, Vec4 color) {
	Draw::DrawSprite({
		.sprite = innerSprite,
		.pos    = pos,
		.z      = Z_HexFill,
		.color  = color,
	});
}

static void DrawBorder(Vec2 pos, Vec4 color) {
	Draw::DrawSprite({
		.sprite = borderSprite,
		.pos    = pos,
		.z      = Z_HexBorder,
		.color  = color,
	});
}

//--------------------------------------------------------------------------------------------------

void Draw(Data* data, DrawDef const* drawDef) {
	Draw::SetCamera({ .pos = cameraPos, .scale = cameraScale });

	for (U32 i = 0; i < data->hexesLen; i++) {
		Hex const* const hex = &data->hexes[i];
		Draw::DrawSprite({
			.sprite = hex->terrain->sprite,
			.pos    = hex->pos,
			.z      = Z_Hex,
		});
	}

	for (Side side = Side_Left; side <= Side_Right; side++) {
		Army const* const army = &data->armies[side];
		for (U32 i = 0; i < army->unitsLen; i++) {
			Unit const* const unit = &army->units[i];
			Draw::DrawSprite({
				.sprite = unit->def->sprite,
				.pos    = unit->pos,
				.z      = Z_Unit,
				.flip   = unit->side == Side_Right,
			});
		}
	}

	for (U16 i = 0; i < MaxHexes; i++) {
		Vec2 const pos = data->hexes[i].pos;
		U64 const flags = drawDef->overlayFlags[i];
		if ((flags & OverlayFlags::FriendlyThreat) && (flags & OverlayFlags::EnemyThreat)) {
			DrawInner(pos, Vec4(1.f, 1.f, 0.f, 0.5f));
		} else if (flags & OverlayFlags::FriendlyThreat) {
			DrawInner(pos, Vec4(0.f, 1.f, 0.f, 0.5f));
		} else if (flags & OverlayFlags::EnemyThreat) {
			DrawInner(pos, Vec4(1.f, 0.f, 0.f, 0.5f));
		}
		if (flags & OverlayFlags::Attacker) {
			DrawBorder(pos, Vec4(1.f, 0.f, 0.f, 1.0f));
		}
	}

	const Draw::Sprite pathSprites[6] = {
		pathTopLeftSprite,
		pathTopRightSprite,
		pathRightSprite,
		pathBottomRightSprite,
		pathBottomLeftSprite,
		pathLeftSprite,
	};
	for (U8 n = 0; n < 6; n++) {
		Draw::Sprite const pathSprite = pathSprites[n];
		for (U64 i = 0; i < drawDef->hoverPath[n].len; i++) {
			Draw::DrawSprite({
				.sprite = pathSprite,
				.pos    = drawDef->hoverPath[n][i],
				.z      = Z_Path,
				.color  = Vec4(1.f, 1.f, 0.f, 0.5f),
			});
		}
		for (U64 i = 0; i < drawDef->targetPath[n].len; i++) {
			Draw::DrawSprite({
				.sprite = pathSprite,
				.pos    = drawDef->targetPath[n][i],
				.z      = Z_Path,
				.color  = Vec4(1.f, 1.f, 0.f, 0.5f),
			});
		}
	}
/*
		     if (flags & HexFlags::HoverAttackTopLeft    ) { dd.pos.x -= (HexSize / 4); dd.pos.y -= HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::HoverAttackTopRight   ) { dd.pos.x += (HexSize / 4); dd.pos.y -= HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::HoverAttackRight      ) { dd.pos.x += (HexSize / 2);                              Draw::DrawSprite(dd); }
		else if (flags & HexFlags::HoverAttackBottomRight) { dd.pos.x += (HexSize / 4); dd.pos.y += HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::HoverAttackBottomLeft ) { dd.pos.x -= (HexSize / 4); dd.pos.y += HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::HoverAttackLeft       ) { dd.pos.x -= (HexSize / 2);                              Draw::DrawSprite(dd); }

		dd.color = TargetAttackColor;
		     if (flags & HexFlags::TargetAttackTopLeft    ) { dd.pos.x -= (HexSize / 4); dd.pos.y -= HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::TargetAttackTopRight   ) { dd.pos.x += (HexSize / 4); dd.pos.y -= HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::TargetAttackRight      ) { dd.pos.x += (HexSize / 2);                              Draw::DrawSprite(dd); }
		else if (flags & HexFlags::TargetAttackBottomRight) { dd.pos.x += (HexSize / 4); dd.pos.y += HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::TargetAttackBottomLeft ) { dd.pos.x -= (HexSize / 4); dd.pos.y += HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::TargetAttackLeft       ) { dd.pos.x -= (HexSize / 2);                              Draw::DrawSprite(dd); }
		Draw::DrawStr({
			.font   = numberFont,
			.str    = SPrintf(tempMem, "%u,%u[%u]", hex->c, hex->r, hex->idx),
			//.str    = SPrintf(tempMem, "%x", hex->flags),
			.pos    = Vec2(hex->pos.x, hex->pos.y + HexSize / 2 - 2.f),
			.z      = Z_Ui + 10,
			.origin = Draw::Origin::BottomCenter,
			//.scale = Vec2(2.f / cameraScale, 2.f / cameraScale),
			.scale = Vec2(1.f / cameraScale, 1.f / cameraScale),
			//.scale = Vec2(1.f, 1.f),
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
		});
		*/

}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle