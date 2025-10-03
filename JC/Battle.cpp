#pragma once

#include "JC/Battle.h"
#include "JC/Draw.h"
#include "JC/Gpu.h"
#include "JC/Math.h"
#include "JC/Window.h"

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

enum struct State {
	StartingBattle = 0,
	EndingBattle,
	StartingRound,
	EndingRound,
	StartingTurn,
	EndingTurn,
	WaitingSelectUnit,
	WaitingOrder,
	ExecutingOrder,
	Max,
};

struct MapPos {
	U16 col;
	U16 row;
};

struct Unit {
	Str          name;
	MapPos       mapPos;
	U8           moves;
	U8           movesRem;
	Vec2         drawPos;
	Draw::Sprite drawSprite;
};

enum struct Team {
	Player1,
	Player2,
	Ai,
	Max,
};

static constexpr U8 MaxArmyUnits = 128;

struct Army {
	Team team;
	Unit units[MaxArmyUnits];
	U16  unitsLen;
};

static constexpr U8 MaxOrderPath = 32;

struct Order {
	Unit*  unit;
	MapPos path[MaxOrderPath];	// includes starting and ending hexes
	U8     pathLen;
	U8     pathIdx;
	U16    durMs;
	U16    elapsedMs;	// current path
};

static Draw::Sprite spearmanSprite;
static State        state;
static Army         armies[(U64)Team::Max];
static Team         currentTeam;
static Unit*        selectedUnit;
static Order        order;

//--------------------------------------------------------------------------------------------------

static U8 hexLut[32 * 24];

static void InitHexLut() {
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

static Vec2 HexToPixel(MapPos p) {
	return {
		(F32)(32 + p.col * 64 + (p.row & 1) * 32),
		(F32)(32 + p.row * 48),
	};
}

static MapPos PixelToHex(Vec2 p) {
	p = Math::Sub(pos, CanvasPos);
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

//--------------------------------------------------------------------------------------------------

Res<> Init() {
	JC_CHECK_RES(Draw::GetSprite("Unit_Spearman").To(spearmanSprite));

	armies[(U32)Team::Player1] = {
		.team = Team::Player1,
		.units = {
			{
				.name       = "Spearman",
				.mapPos     = { 0, 0 },
				.moves      = 3,
				.movesRem   = 3,
				.drawPos    = HexToPixel({ 0, 0 }),
				.drawSprite = spearmanSprite,
			},
		},
		.unitsLen = 1,
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Events(Span<const Window::Event> events) {
	for (U64 i = 0; i < events.len; i++) {
		const Window::Event* const event = &events[i];

		switch (state) {
			case State::WaitingSelectUnit:
				if (event->type == Window::EventType::Key && event->key.down && event->key.key == Window::Key::Mouse1) {
					// get mouse pos
					// get hex for mouse pos
					// check if hex's unit is on the current team and has any moves left
					// if okay, selectedUnit = hex->unit;
					// state = State::WaitingUnitOrder;
				}
				break;

			case State::WaitingOrder:
				if (event->type == Window::EventType::Key && event->key.down && event->key.key == Window::Key::Mouse1) {
					// get mouse pos
					// get hex for mouse pos
					// if empty hex:
					selectedUnit = 0;
					state = State::WaitingSelectUnit;
				} else if (event->type == Window::EventType::Key && event->key.down && event->key.key == Window::Key::Mouse2) {
					// get mouse pos
					// get hex for mouse pos
					// if hex is empty and exists a path from selectedUnit's pos to target hex
						// set up move order
					state = State::ExecutingOrder;
				}
				break;

			case State::ExecutingOrder:
				// no input
				break;
		}
	}
}

//--------------------------------------------------------------------------------------------------

void Update(U64 ticks) {
	if (state != State::ExecutingOrder) {
		return;
	}
	//update move animation
}

//--------------------------------------------------------------------------------------------------

void Draw(Gpu::Frame frame) {
	// map
	// units
	// selected unit
	// target hex
	// path
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle