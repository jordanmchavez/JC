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
static Draw::Sprite highlightSprite;
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
	TryTo(Draw::GetSprite(battleJson->borderSprite),           borderSprite);
	TryTo(Draw::GetSprite(battleJson->highlightSprite),        highlightSprite);
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
static constexpr Vec4 PathColor         = Vec4(1.f, 1.f, 0.f, 1.f);
static constexpr Vec4 ArrowColor        = Vec4(1.f, 0.f, 0.f, 1.f);

static constexpr F32 Z_Background   = 0.0f;
static constexpr F32 Z_Hex          = 1.0f;
static constexpr F32 Z_Path         = 1.01f;
static constexpr F32 Z_HexFill      = 1.02f;
static constexpr F32 Z_HexBorder    = 1.03f;
static constexpr F32 Z_Arrow        = 1.04f;
static constexpr F32 Z_HexUi        = 1.05f;
static constexpr F32 Z_Unit         = 2.0f;
static constexpr F32 Z_UiBackground = 3.0f;
static constexpr F32 Z_Ui           = 3.1f;

void DrawHex(Hex const* hex, Draw::Sprite sprite, F32 z, Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f)) {
	Draw::DrawSprite({
		.sprite = sprite,
		.pos    = hex->pos,
		.z      = z,
		.origin = Draw::Origin::Center,
		.color  = color,
	});
}

struct DrawDef {
	Hex const* friendlyMoveableHexes[MaxCols * MaxRows];
	U32        friendlyMoveableHexesLen;
	Hex const* enemyAttackableHexes[MaxCols * MaxRows];
	U32        enemyAttackableHexesLen;
};

void Draw(Data const* data) {
	Draw::SetCamera({ .pos = data->cameraPos, .scale = data->cameraScale });

	for (U32 i = 0; i < data->hexesLen; i++) {
		Hex const* const hex = &data->hexes[i];
		DrawHex(hex, hex->terrain->sprite, Z_Hex);

		U64 const flags = hex->flags;
		if (flags & HexFlags::EnemyAttacker) { DrawHex(hex, highlightSprite, Z_HexFill, Vec4(1.f, 0.f, 0.f, 1.f)); }
		else if ((flags & HexFlags::FriendlyMoveableOrAttackable) && (flags & HexFlags::EnemyAttackable)) { DrawHex(hex, highlightSprite, Z_HexFill, Vec4(1.f, 1.f, 0.f, 0.5f)); }
		else if (flags & HexFlags::FriendlyMoveableOrAttackable) { DrawHex(hex, highlightSprite, Z_HexFill, Vec4(0.f, 1.f, 0.f, 0.5f)); }
		else if (flags & HexFlags::EnemyAttackable) { DrawHex(hex, highlightSprite, Z_HexFill, Vec4(1.f, 0.f, 0.f, 0.5f)); }

		if (flags & HexFlags::PathTopLeft    ) { DrawHex(hex, pathTopLeftSprite,     Z_Path, PathColor); }
		if (flags & HexFlags::PathTopRight   ) { DrawHex(hex, pathTopRightSprite,    Z_Path, PathColor); }
		if (flags & HexFlags::PathRight      ) { DrawHex(hex, pathRightSprite,       Z_Path, PathColor); }
		if (flags & HexFlags::PathBottomRight) { DrawHex(hex, pathBottomRightSprite, Z_Path, PathColor); }
		if (flags & HexFlags::PathBottomLeft ) { DrawHex(hex, pathBottomLeftSprite,  Z_Path, PathColor); }
		if (flags & HexFlags::PathLeft       ) { DrawHex(hex, pathLeftSprite,        Z_Path, PathColor); }

		Draw::SpriteDrawDef dd = {
			.sprite = attackSprite,
			.pos    = hex->pos,
			.z      = Z_Arrow,
			.origin = Draw::Origin::Center,
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
		};
		     if (flags & HexFlags::AttackTopLeft    ) { dd.pos.x -= (HexSize / 4); dd.pos.y -= HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::AttackTopRight   ) { dd.pos.x += (HexSize / 4); dd.pos.y -= HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::AttackRight      ) { dd.pos.x += (HexSize / 2);                              Draw::DrawSprite(dd); }
		else if (flags & HexFlags::AttackBottomRight) { dd.pos.x += (HexSize / 4); dd.pos.y += HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::AttackBottomLeft ) { dd.pos.x -= (HexSize / 4); dd.pos.y += HexSize * 3 / 8; Draw::DrawSprite(dd); }
		else if (flags & HexFlags::AttackLeft       ) { dd.pos.x -= (HexSize / 2);                              Draw::DrawSprite(dd); }

		if (flags & HexFlags::Selected) { DrawHex(hex, borderSprite, Z_HexBorder, SelectedHexColor); }
		else if (flags & HexFlags::SelectedAttackTarget) { DrawHex(hex, borderSprite, Z_HexBorder, Vec4(1.f, 0.f, 0.f, 1.f)); }
		else if (flags & HexFlags::HoverValid) { DrawHex(hex, borderSprite, Z_HexBorder, Vec4(1.f, 1.f, 1.f, 1.f)); }
		else if (flags & HexFlags::HoverInvalid) { DrawHex(hex, borderSprite, Z_HexBorder, Vec4(1.f, 1.f, 1.f, 0.5f)); }

		Draw::DrawStr({
			.font   = numberFont,
			.str    = SPrintf(tempMem, "%u,%u[%u]", hex->c, hex->r, hex->idx),
			//.str    = SPrintf(tempMem, "%x", hex->flags),
			.pos    = Vec2(hex->pos.x, hex->pos.y + HexSize / 2 - 2.f),
			.z      = Z_Ui + 10,
			.origin = Draw::Origin::BottomCenter,
			//.scale = Vec2(2.f / data->cameraScale, 2.f / data->cameraScale),
			.scale = Vec2(1.f / data->cameraScale, 1.f / data->cameraScale),
			//.scale = Vec2(1.f, 1.f),
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
		});

	}

	for (Side side = Side::Left; side <= Side::Right; side++) {
		Army const* const army = &data->armies[side];
		for (U32 i = 0; i < army->unitsLen; i++) {
			Unit const* const unit = &army->units[i];
			Draw::DrawSprite({
				.sprite = unit->def->sprite,
				.pos    = unit->pos,
				.z      = Z_Unit,
				.origin = Draw::Origin::Center,
				.flip   = unit->side == Side::Right,
			});
		}
	}

}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle