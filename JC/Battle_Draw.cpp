#include "JC/Battle_Internal.h"

#include "JC/Draw.h"
#include "JC/Log.h"
#include "JC/UnitDef.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

static constexpr F32 CameraSpeedPixelsPerSec = 1000.f;

static constexpr F32 Z_Background   = 0.00f;
static constexpr F32 Z_Hex          = 1.01f;
static constexpr F32 Z_HexInner     = 1.02f;
static constexpr F32 Z_HexBorder    = 1.03f;
static constexpr F32 Z_HoverPath    = 1.04f;
static constexpr F32 Z_TargetPath   = 1.05f;
static constexpr F32 Z_Unit         = 1.06f;
static constexpr F32 Z_HoverAttack  = 1.07f;
static constexpr F32 Z_TargetAttack = 1.08f;
static constexpr F32 Z_HexUi        = 1.09f;
static constexpr F32 Z_UiBackground = 3.00f;
static constexpr F32 Z_Ui           = 3.01f;

struct DrawTypeData {
	Draw::Sprite sprite;
	F32          z;
	Vec4         color;
};

static Mem          tempMem;
static Vec2         windowSize;
static Draw::Sprite emptySprite;
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
static DrawTypeData drawTypeData[DrawType_Max];

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
	TryTo(Draw::GetSprite("Empty"), emptySprite);

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

	drawTypeData[DrawType_None                              ] = { .sprite = emptySprite,           .z = -1.f,           .color = Vec4(0.f, 0.f, 0.f, 0.0f) };
	drawTypeData[DrawType_FriendlyMoveable                  ] = { .sprite = innerSprite,           .z = Z_HexInner,     .color = Vec4(0.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_FriendlyAttackable                ] = { .sprite = innerSprite,           .z = Z_HexInner,     .color = Vec4(0.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_EnemyAttackable                   ] = { .sprite = innerSprite,           .z = Z_HexInner,     .color = Vec4(1.f, 0.f, 0.f, 0.5f) };
	drawTypeData[DrawType_FriendlyMoveableAndEnemyAttackable] = { .sprite = innerSprite,           .z = Z_HexInner,     .color = Vec4(1.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_EnemyAttacker                     ] = { .sprite = borderSprite,          .z = Z_HexBorder,    .color = Vec4(1.f, 0.f, 0.f, 1.0f) };
	drawTypeData[DrawType_HoverDefault                      ] = { .sprite = borderSprite,          .z = Z_HexBorder,    .color = Vec4(1.f, 1.f, 1.f, 0.5f) };
	drawTypeData[DrawType_HoverMoveable                     ] = { .sprite = borderSprite,          .z = Z_HexBorder,    .color = Vec4(1.f, 1.f, 1.f, 1.0f) };
	drawTypeData[DrawType_HoverSelectableFriendly           ] = { .sprite = borderSprite,          .z = Z_HexBorder,    .color = Vec4(1.f, 1.f, 1.f, 1.0f) };
	drawTypeData[DrawType_HoverAttackableEnemy              ] = { .sprite = borderSprite,          .z = Z_HexBorder,    .color = Vec4(1.f, 1.f, 1.f, 1.0f) };
	drawTypeData[DrawType_HoverUnattackableEnemy            ] = { .sprite = borderSprite,          .z = Z_HexBorder,    .color = Vec4(1.f, 1.f, 1.f, 0.5f) };
	drawTypeData[DrawType_Selected                          ] = { .sprite = borderSprite,          .z = Z_HexBorder,    .color = Vec4(0.f, 1.f, 0.f, 1.0f) };
	drawTypeData[DrawType_HoverPathTopLeft                  ] = { .sprite = pathTopLeftSprite,     .z = Z_HoverPath,    .color = Vec4(1.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_HoverPathTopRight                 ] = { .sprite = pathTopRightSprite,    .z = Z_HoverPath,    .color = Vec4(1.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_HoverPathRight                    ] = { .sprite = pathRightSprite,       .z = Z_HoverPath,    .color = Vec4(1.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_HoverPathBottomRight              ] = { .sprite = pathBottomRightSprite, .z = Z_HoverPath,    .color = Vec4(1.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_HoverPathBottomLeft               ] = { .sprite = pathBottomLeftSprite,  .z = Z_HoverPath,    .color = Vec4(1.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_HoverPathLeft                     ] = { .sprite = pathLeftSprite,        .z = Z_HoverPath,    .color = Vec4(1.f, 1.f, 0.f, 0.5f) };
	drawTypeData[DrawType_HoverAttack                       ] = { .sprite = attackSprite,          .z = Z_HoverAttack,  .color = Vec4(1.f, 1.f, 1.f, 0.5f) };
	drawTypeData[DrawType_TargetPathTopLeft                 ] = { .sprite = pathTopLeftSprite,     .z = Z_TargetPath,   .color = Vec4(1.f, 1.f, 0.f, 1.0f) };
	drawTypeData[DrawType_TargetPathTopRight                ] = { .sprite = pathTopRightSprite,    .z = Z_TargetPath,   .color = Vec4(1.f, 1.f, 0.f, 1.0f) };
	drawTypeData[DrawType_TargetPathRight                   ] = { .sprite = pathRightSprite,       .z = Z_TargetPath,   .color = Vec4(1.f, 1.f, 0.f, 1.0f) };
	drawTypeData[DrawType_TargetPathBottomRight             ] = { .sprite = pathBottomRightSprite, .z = Z_TargetPath,   .color = Vec4(1.f, 1.f, 0.f, 1.0f) };
	drawTypeData[DrawType_TargetPathBottomLeft              ] = { .sprite = pathBottomLeftSprite,  .z = Z_TargetPath,   .color = Vec4(1.f, 1.f, 0.f, 1.0f) };
	drawTypeData[DrawType_TargetPathLeft                    ] = { .sprite = pathLeftSprite,        .z = Z_TargetPath,   .color = Vec4(1.f, 1.f, 0.f, 1.0f) };
	drawTypeData[DrawType_TargetAttack                      ] = { .sprite = attackSprite,          .z = Z_TargetAttack, .color = Vec4(1.f, 1.f, 1.f, 1.0f) };

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

static void DrawObjs(DrawObj const* drawObjs, U16 len) {
	for (U64 i = 0; i < len; i++) {
		DrawTypeData const* const data = &drawTypeData[drawObjs[i].type];
		Draw::DrawSprite({
			.sprite = data->sprite,
			.pos    = drawObjs[i].pos,
			.z      = data->z,
			.color  = data->color,
		});
	}
}

//--------------------------------------------------------------------------------------------------

void Draw(Data const* data, DrawDef const* drawDef) {
	Draw::SetCamera({ .pos = cameraPos, .scale = cameraScale });

	for (U32 i = 0; i < data->hexesLen; i++) {
		Hex const* const hex = &data->hexes[i];
		Draw::DrawSprite({
			.sprite = hex->terrain->sprite,
			.pos    = hex->pos,
			.z      = Z_Hex,
		});

		Draw::DrawStr({
			.font   = numberFont,
			.str    = SPrintf(tempMem, "%u,%u[%u]", hex->c, hex->r, hex->idx),
			.pos    = Vec2(hex->pos.x, hex->pos.y + HexSize / 2 - 2.f),
			.z      = Z_Ui + 10,
			.origin = Draw::Origin::BottomCenter,
			.scale = Vec2(1.f / cameraScale, 1.f / cameraScale),
			.color  = Vec4(1.f, 1.f, 1.f, 1.f),
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

	DrawObjs(drawDef->overlay, drawDef->overlayLen);
	DrawObjs(&drawDef->hover, 1);
	DrawObjs(&drawDef->selected, 1);
	DrawObjs(drawDef->hoverPath, drawDef->hoverPathLen);
	DrawObjs(drawDef->targetPath, drawDef->targetPathLen);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle