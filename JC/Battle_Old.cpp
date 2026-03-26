
enum struct State {
	NoOrderUnitSelected = 0,
	OrderUnitSelected,
	OrderMoveLocked,
	OrderTargetLocked,
	OrderExecuting,
};

static constexpr U16 MaxOrderSteps = 512;

enum struct OrderStepType {
	None = 0,
	MoveThrough,
	MoveOnto,
	AttackIn,
	AttackOut,
	Die,
};

struct OrderStep {
	OrderStepType type;
	Hex*          fromHex;
	Hex*          toHex;
	Unit*         unit;
	F32           durSec;
};

struct Order {
	Unit*     unit;
	OrderStep steps[MaxOrderSteps];
	U16       stepsLen;
	U16       stepsIdx;
	F32       elapsedSec;
};

static constexpr U32 MaxTerrain = 64;

static Mem                tempMem;
static Shared*            shared;
static Terrain*           terrain;
static U32                terrainLen;
static Map<Str, Terrain*> terrainMap;

static Hex*  hoverHex;
static Unit* orderUnit;
static Path  orderPath;
static Hex*  orderTargetHex;
static bool  rebuildOverlay;

static bool showEnemyArmyThreatMap;
static DrawDef drawDef;

static State state;

static Order order;
static Draw::Font numberFont;

//--------------------------------------------------------------------------------------------------

Json_Begin(BattleDef)
	Json_Member("atlasPaths",             atlasPaths)
	Json_Member("numberFont",             numberFont)
	Json_Member("uiFont",                 uiFont)
	Json_Member("fancyFont",              fancyFont)
	Json_Member("borderSprite",           borderSprite)
	Json_Member("innerSprite",            innerSprite)
	Json_Member("pathTopLeftSprite",      pathTopLeftSprite)
	Json_Member("pathTopRightSprite",     pathTopRightSprite)
	Json_Member("pathRightSprite",        pathRightSprite)
	Json_Member("pathBottomRightSprite",  pathBottomRightSprite)
	Json_Member("pathBottomLeftSprite",   pathBottomLeftSprite)
	Json_Member("pathLeftSprite",         pathLeftSprite)
	Json_Member("arrowTopLeftSprite",     arrowTopLeftSprite)
	Json_Member("arrowTopRightSprite",    arrowTopRightSprite)
	Json_Member("arrowRightSprite",       arrowRightSprite)
	Json_Member("arrowBottomRightSprite", arrowBottomRightSprite)
	Json_Member("arrowBottomLeftSprite",  arrowBottomLeftSprite)
	Json_Member("arrowLeftSprite",        arrowLeftSprite)
Json_End(BattleDef)


static U32 Damage(U32 base) {
	U32 lo10    = base * 8;				// Fixed point 10
	U32 hi10    = base * 12;			// Fixed point 10
	U32 lo      = lo10 / 10;			// Damage floor
	U32 hi      = (hi10 + 9) / 10;		// Damage ceiling
	U32 loWidth = (lo + 1) * 10 - lo10;	// Width of low bucket
	U32 hiWidth = hi10 - (hi - 1) * 10;	// Width of high bucket

	U32 total = loWidth + hiWidth + (hi - lo - 1) * 10;
	U32 roll = Rng::NextU32(0, total - 1);
	if (roll < loWidth) { return lo; }
	roll -= loWidth;
	U32 interior = roll / 10;
	if (interior < (hi - lo - 1)) { return lo + 1 + interior; }
	return hi;
}

//--------------------------------------------------------------------------------------------------

static void BuildAttackMap(Army* army) {
	U64* const attackMap = army->attackMap;
	memset(attackMap, 0, sizeof(attackMap));
	for (U32 i = 0; i < army->unitsLen; i++) {
		Unit* const unit      = &army->units[i];
		U8    const unitIdx   = (U8)(unit - army->units);
		U64   const unitBit   = (U64)1 << (U64)unitIdx;
		for (U16 j = 0; j < MaxHexes; j++) {
			attackMap[j] &= ~unitBit;
			if (unit->acted) { continue; }
			if (unit->pathMap.parents[j]) {
				attackMap[j] |= unitBit;
				continue;
			}
			Hex const* hex = &shared->hexes[j];
			for (U8 dir = 0; dir < 6; dir++) {
				Hex const* const neighbor = hex->neighbors[dir];
				if (
					neighbor &&
					!neighbor->unit &&
					unit->pathMap.parents[neighbor->idx] &&
					HexDistance(neighbor, hex) <= unit->range
				) {
					attackMap[j] |= unitBit;
					break;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

{
	U32 startCol = 0;
	for (Side side = Side_Left; side <= Side_Right; side++) {
		Logf("Creating side %u", (U32)side);
		Army* army = &shared->armies[(U32)side];
		memset(army, 0, sizeof(Army));
		army->side = side;
		for (U32 c = startCol; c < startCol + 2; c++) {
			for (U32 r = 0; r < MaxRows; r++) {
				Hex* const hex = &shared->hexes[c + (r * MaxCols)];
				if (!hex->unit&& Rng::NextU32(0, 100) < 50) {
					Unit::Unit unit; TryTo(Unit::Create("Spearmen"), unit);
					Unit* const unit = &army->units[army->unitsLen++];
					*unit = {
						.def   = def,
						.hex   = hex,
						.pos   = hex->pos,
						.side  = side,
						.hp    = def->hp,
						.move  = def->move,
						.range = 1,
					};
					hex->unit = unit;
					Logf("Created unit for side %u at %u,%u", (U32)side, c, r);
					if (army->unitsLen >= 12) {
						goto DoneArmyGen;
					}
				}
			}
		}
		DoneArmyGen:
		startCol += 2;
	}

	for (Side side = Side_Left; side <= Side_Right; side++) {
		Army* army = &shared->armies[(U32)side];
		for (U32 i = 0; i < army->unitsLen; i++) {
			BuildPathMap(shared->hexes, &army->units[i]);
		}
	}

	for (Side side = Side_Left; side <= Side_Right; side++) {
		BuildAttackMap(&shared->armies[(U32)side]);
	}

	shared->activeSide = Side_Left;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

struct PathPrinter : Printer {
	Path const* path;

	PathPrinter(Path const* pathIn) { path = pathIn; }

	void Print(StrBuf* sb) override {
		sb->Add('[');
		U16 i = 0;
		for (; i < path->len; i++) {
			sb->Printf("(%u,%u), ", path->hexes[i]->c, path->hexes[i]->r);
		}
		if (i > 0) { sb->Remove(2); }
		sb->Add(']');
	}
};

//--------------------------------------------------------------------------------------------------

static void CreateOrder(Unit* unit, Path* movePath, Hex* targetHex) {
	memset(&order, 0, sizeof(order));

	order.unit = unit;
	Hex* fromHex = order.unit->hex;
	for (U16 i = 0; i < movePath->len - 1; i++) {
		order.steps[order.stepsLen++] = {
			.type    = OrderStepType::MoveThrough,
			.fromHex = fromHex,
			.toHex   = movePath->hexes[i],
			.durSec  = 0.5f,
		};
		fromHex = movePath->hexes[i];
	}
	if (movePath->len) {
		order.steps[order.stepsLen++] = {
			.type    = OrderStepType::MoveOnto,
			.fromHex = fromHex,
			.toHex   = movePath->hexes[movePath->len - 1],
			.durSec  = 0.5f,
		};
	}
	if (targetHex) {
		Assert(targetHex->unit);
		order.steps[order.stepsLen++] = {
			.type    = OrderStepType::AttackIn,
			.unit   = targetHex->unit,
			.durSec = 0.5f,
		};
		order.steps[order.stepsLen++] = {
			.type   = OrderStepType::AttackOut,
			.unit   = targetHex->unit,
			.durSec = 0.5f,
		};
	}

	state = State::OrderExecuting;

	orderUnit             = nullptr;
	orderPath.len         = 0;
	orderTargetHex        = nullptr;
	hoverHex              = nullptr;
	drawDef.overlayLen    = 0;
	drawDef.selected.type = DrawType_None;
	drawDef.pathLen       = 0;

	Logf("Executing order");
}

//--------------------------------------------------------------------------------------------------

static U8 GetDir(Hex const* from, Hex const* to) {
	if (from->neighbors[Dir::TopLeft    ] == to) { return Dir::TopLeft; }
	if (from->neighbors[Dir::TopRight   ] == to) { return Dir::TopRight; }
	if (from->neighbors[Dir::Right      ] == to) { return Dir::Right; }
	if (from->neighbors[Dir::BottomRight] == to) { return Dir::BottomRight; }
	if (from->neighbors[Dir::BottomLeft ] == to) { return Dir::BottomLeft; }
	if (from->neighbors[Dir::Left       ] == to) { return Dir::Left; }
	Panic("Neighbors not adjacent");
}

//--------------------------------------------------------------------------------------------------

static Vec2 GetBorderPosForDir(Hex const* hex, U8 dir) {
	switch (dir) {
		case Dir::TopLeft    : return Vec2(hex->pos.x - (HexSize / 4), hex->pos.y - HexSize * 3 / 8);
		case Dir::TopRight   : return Vec2(hex->pos.x + (HexSize / 4), hex->pos.y - HexSize * 3 / 8);
		case Dir::Right      : return Vec2(hex->pos.x + (HexSize / 2), hex->pos.y                  );
		case Dir::BottomRight: return Vec2(hex->pos.x + (HexSize / 4), hex->pos.y + HexSize * 3 / 8);
		case Dir::BottomLeft : return Vec2(hex->pos.x - (HexSize / 4), hex->pos.y + HexSize * 3 / 8);
		case Dir::Left       : return Vec2(hex->pos.x - (HexSize / 2), hex->pos.y                  );
	}
	Panic("Neighbors not adjacent");
}

static Vec2 GetBorderPosBetween(Hex const* from, Hex const* to) {
	if (from->neighbors[Dir::TopLeft    ] == to) { return Vec2(from->pos.x - (HexSize / 4), from->pos.y - HexSize * 3 / 8); }
	if (from->neighbors[Dir::TopRight   ] == to) { return Vec2(from->pos.x + (HexSize / 4), from->pos.y - HexSize * 3 / 8); }
	if (from->neighbors[Dir::Right      ] == to) { return Vec2(from->pos.x + (HexSize / 2), from->pos.y                  ); }
	if (from->neighbors[Dir::BottomRight] == to) { return Vec2(from->pos.x + (HexSize / 4), from->pos.y + HexSize * 3 / 8); }
	if (from->neighbors[Dir::BottomLeft ] == to) { return Vec2(from->pos.x - (HexSize / 4), from->pos.y + HexSize * 3 / 8); }
	if (from->neighbors[Dir::Left       ] == to) { return Vec2(from->pos.x - (HexSize / 2), from->pos.y                  ); }
	Panic("Neighbors not adjacent");
}

//--------------------------------------------------------------------------------------------------

static void BuildDrawPath(DrawType pathBaseDrawType, Hex* attackHex, DrawType attackDrawType) {
	drawDef.pathLen = 0;
	Hex const* from = orderUnit->hex;
	for (U16 i = 0; i < orderPath.len; i++) {
		Hex const* to = orderPath.hexes[i];
		U8 const dir = GetDir(from, to);
		drawDef.path[drawDef.pathLen++] = {
			.pos  = from->pos,
			.type = (DrawType)(pathBaseDrawType + dir),
		};
		drawDef.path[drawDef.pathLen++] = {
			.pos  = to->pos,
			.type = (DrawType)(pathBaseDrawType + ((dir + 3) % 6)),	// opposite side
		};
		from = to;
	}

	if (attackHex) {
		U8 const dir = GetDir(from, attackHex);
		drawDef.path[drawDef.pathLen++] = {
			.pos  = from->pos,
			.type = (DrawType)(pathBaseDrawType + dir),
		};
		drawDef.path[drawDef.pathLen++] = {
			.pos  = GetBorderPosForDir(from, dir),
			.type = attackDrawType,
		};
	}
}

//--------------------------------------------------------------------------------------------------

static void LockOrderMove(Hex* moveHex) {
	orderTargetHex = nullptr;
	if (!orderPath.len) {
		BuildPathOrPanic(orderUnit, moveHex, &orderPath);
	}
	BuildDrawPath(DrawType_TargetPathBase, nullptr, DrawType_None);
	state = State::OrderMoveLocked;
	Logf("Targetted (%u, %u) for move", moveHex->c, moveHex->r);
}

static void LockOrderTarget(Hex* targetHex) {
	orderTargetHex = targetHex;
	BuildDrawPath(DrawType_TargetPathBase, targetHex, DrawType_TargetAttack);
	state = State::OrderTargetLocked;
	Logf("Targetted (%u, %u) for attack", targetHex->c, targetHex->r);
}

//--------------------------------------------------------------------------------------------------

static void SelectOrderUnit(Unit* unit) {
	if (orderUnit == unit) { return; }
	orderTargetHex        = nullptr;
	orderUnit             = unit;
	orderPath.len         = 0;
	rebuildOverlay        = true;
	drawDef.pathLen       = 0;
	drawDef.selected.pos  = orderUnit->hex->pos;
	drawDef.selected.type = DrawType_Selected;
	state                 = State::OrderUnitSelected;
	Logf("Selected (%u, %u)", unit->hex->c, unit->hex->r);
}

//--------------------------------------------------------------------------------------------------

static void UpdateHover();
static void RebuildOverlay();

void SelectNextUnit() {
	Army* const army = &shared->armies[shared->activeSide];
	if (army->unitsLen == 0) { return; }
	U8 idx = 0;
	if (orderUnit) {
		idx = (U8)(orderUnit - army->units);
	}
	U8 i = idx;
	for (;;) {
		i = (i + 1) % army->unitsLen;
		if (i == idx) { break; }
		if (!army->units[i].acted) {
			SelectOrderUnit(&army->units[i]);
			break;
		}
	}
	if (i == idx) {
		orderUnit = nullptr;
		orderPath.len         = 0;
		drawDef.pathLen = 0;
		drawDef.selected.type = DrawType_None;
		state                 = State::NoOrderUnitSelected;

	}
	UpdateHover();
	RebuildOverlay();
}

//--------------------------------------------------------------------------------------------------

void EndSelectedUnitTurn() {
	if (orderUnit && !orderUnit->acted) {
		orderUnit->acted = true;
		orderUnit->move = 0;
		BuildPathMap(shared->hexes, orderUnit);
		BuildAttackMap(&shared->armies[orderUnit->side]);
		SelectNextUnit();
	}
}

//--------------------------------------------------------------------------------------------------

static bool Friendly(Unit const* unit) {
	return unit->side == shared->activeSide;
}

static bool Enemy(Unit const* unit) {
	return unit->side != shared->activeSide;
}

static bool Selectable(Unit const* unit) {
	return !unit->acted && Friendly(unit);
}

static bool InRangeAtEndOfPath(Unit const* unit, Hex const* targetHex) {
	return orderPath.len && HexDistance(orderPath.hexes[orderPath.len - 1], targetHex) <= unit->range;
}

static bool Attackable(Unit const* attacker, Unit const* target) {
	return
		!attacker->acted &&
		attacker->side != target->side &&
		(InRangeAtEndOfPath(attacker, target->hex) || AreHexesAdjacent(attacker->hex, target->hex));
}

static bool Moveable(Unit const* unit, Hex const* hex) {
	// !unit->acted is redundant since an acted unit should have its movement set to zero and its pathmap cleared
	// So use an assert for sanity check. Not comprehensive, but might still catch something
	if (unit->acted) {
		Assert(!unit->pathMap.parents[hex->idx]);
	}
	return !hex->unit && unit->pathMap.parents[hex->idx];
}

static Hex* EndOfPathHex() {
	return (orderPath.len > 0) ? orderPath.hexes[orderPath.len - 1] : nullptr;
}

//--------------------------------------------------------------------------------------------------

void LeftClick(Hex* clickHex) {
	Unit* const clickUnit = clickHex->unit;

	if (state == State::NoOrderUnitSelected) {
		if (clickUnit && Selectable(clickUnit)) {
			SelectOrderUnit(clickUnit);
		}

	} else if (state == State::OrderUnitSelected) {
		if (clickUnit) {
			if (Selectable(clickUnit)) {
				SelectOrderUnit(clickUnit);
			} else if (Attackable(orderUnit, clickUnit)) {
				LockOrderTarget(clickHex);
			}

		} else if (Moveable(orderUnit, clickHex)) {
			LockOrderMove(clickHex);
		}

	} else if (state == State::OrderMoveLocked) {
		if (clickHex == EndOfPathHex()) {
			CreateOrder(orderUnit, &orderPath, nullptr);
		} else if (clickUnit && Enemy(clickUnit) && InRangeAtEndOfPath(orderUnit, clickHex)) {
			LockOrderTarget(clickHex);
		}

	} else if (state == State::OrderTargetLocked) {
		if (clickUnit == orderTargetHex->unit) {
			CreateOrder(orderUnit, &orderPath, orderTargetHex);
		} else if (clickUnit && Enemy(clickUnit) && InRangeAtEndOfPath(orderUnit, clickHex)) {
			LockOrderTarget(clickHex);
		}
	}
}

//--------------------------------------------------------------------------------------------------

static void RightClick() {
	if (state == State::OrderUnitSelected) {
		orderUnit             = nullptr;
		orderPath.len         = 0;
		rebuildOverlay        = true;
		drawDef.pathLen       = 0;
		drawDef.selected.type = DrawType_None;
		state                 = State::NoOrderUnitSelected;
		Logf("Cleared order selected unit");

	} else if (state == State::OrderMoveLocked || state == State::OrderTargetLocked) {
		orderTargetHex  = nullptr;
		state           = State::OrderUnitSelected;
		drawDef.pathLen = 0;
		UpdateHover();
		Logf("Cleared order locked");
	}
}

//--------------------------------------------------------------------------------------------------

static void UpdateHover() {
	drawDef.hover.type = DrawType_None;
	if (!hoverHex) { return; }
	rebuildOverlay = true;
	drawDef.hover.pos  = hoverHex->pos;
	drawDef.hover.type = DrawType_HoverNoninteractible;

	Unit* const hoverUnit = hoverHex->unit;
	if (state == State::NoOrderUnitSelected) {
		if (hoverUnit && Selectable(hoverUnit)) {
			drawDef.hover.type = DrawType_HoverInteractible;
			Logf("Hover selectable friendly");
		}

	} else if (state == State::OrderUnitSelected) {
		if (hoverUnit) {
			if (Selectable(hoverUnit)) {
				if (hoverUnit == orderUnit) {
					orderPath.len   = 0;
					drawDef.pathLen = 0;
				}
				drawDef.hover.type = DrawType_HoverInteractible;
				Logf("Hover selectable friendly");

			} else if (Attackable(orderUnit, hoverUnit)) {
				drawDef.hover.type = DrawType_HoverInteractible;
				BuildDrawPath(DrawType_HoverPathBase, hoverHex, DrawType_HoverAttack);
				Logf("Hover attackable enemy ");
			}

		} else if (Moveable(orderUnit, hoverHex)) {
			BuildPathOrPanic(orderUnit, hoverHex, &orderPath);
			drawDef.hover.type = DrawType_HoverInteractible;
			BuildDrawPath(DrawType_HoverPathBase, nullptr, DrawType_None);
			Logf("Hover moveable");
		}
	}

	else if (state == State::OrderMoveLocked || state == State::OrderTargetLocked) {
		if (hoverUnit) {
			if (Friendly(hoverUnit)) {
				drawDef.hover.type = DrawType_HoverInteractible;
				Logf("Hover selecteable friendly");
			} else if (InRangeAtEndOfPath(orderUnit, hoverHex)) {
				drawDef.hover.type = DrawType_HoverInteractible;
				Logf("Hover attackable from end of target path");
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

void RebuildOverlay() {
	drawDef.overlayLen = 0;

	Army const* const friendlyArmy   = &shared->armies[shared->activeSide];
	Army const* const enemyArmy      = &shared->armies[1 - shared->activeSide];
	U64 const* const  enemyAttackMap = enemyArmy->attackMap;

	Unit const* friendlyUnit = nullptr;
	Unit const* enemyUnit    = nullptr;
	if (orderUnit) {
		friendlyUnit = orderUnit;
	} else if (hoverHex && hoverHex->unit) {
		if (hoverHex->unit->side == shared->activeSide) {
			friendlyUnit = hoverHex->unit;
		} else {
			enemyUnit = hoverHex->unit;
		}
	}

	if (showEnemyArmyThreatMap) {
		U16 dummyMoveCosts[MaxHexes];
		U16 const* moveCosts = nullptr;
		if (friendlyUnit) {
			moveCosts = friendlyUnit->pathMap.moveCosts;
		} else {
			memset(&dummyMoveCosts, 0xff, sizeof(dummyMoveCosts));
			moveCosts = dummyMoveCosts;
		}

		for (U16 i = 0; i < MaxHexes; i++) {
			Unit const* hexUnit = shared->hexes[i].unit;
			bool const friendlyMoveable = moveCosts[i] != U16Max && (!hexUnit || hexUnit->side != shared->activeSide);
			bool const enemyAttackable  = enemyAttackMap[i] != 0;
			if (friendlyMoveable && enemyAttackable) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = shared->hexes[i].pos,
					.type = DrawType_FriendlyMoveableAndEnemyAttackable,
				};
			} else if (friendlyMoveable) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = shared->hexes[i].pos,
					.type = DrawType_FriendlyMoveable,
				};
			} else if (enemyAttackable) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = shared->hexes[i].pos,
					.type = DrawType_EnemyAttackable,
				};
			}
		}
		if (hoverHex) {
			U64 const enemyAttackBits = enemyArmy->attackMap[hoverHex->idx];
			for (U8 i = 0; i < enemyArmy->unitsLen; i++) {
				if (enemyAttackBits & ((U64)1 << i)) {
					drawDef.overlay[drawDef.overlayLen++] = {
						.pos  = enemyArmy->units[i].hex->pos,
						.type = DrawType_EnemyAttacker,
					};
				}
			}
		}

	// no enemy threat map
	} else if (friendlyUnit) {
		U64 const friendlyUnitBit = (U64)1 << (U64)(friendlyUnit - friendlyArmy->units);
		for (U16 i = 0; i < MaxHexes; i++) {
			Unit const* const hexUnit = shared->hexes[i].unit;
			if (hexUnit && hexUnit->side == shared->activeSide) { continue; }
			if (friendlyUnit->pathMap.parents[i]) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = shared->hexes[i].pos,
					.type = DrawType_FriendlyMoveable,
				};
			} else if (hexUnit && hexUnit->side != shared->activeSide && (friendlyArmy->attackMap[i] & friendlyUnitBit)) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = shared->hexes[i].pos,
					.type = DrawType_FriendlyAttackable,
				};
			}
		}
	} else if (enemyUnit) {
		U64 const enemyUnitBit = (U64)1 << (U64)(enemyUnit - enemyArmy->units);
		for (U16 i = 0; i < MaxHexes; i++) {
			if (enemyArmy->attackMap[i] & enemyUnitBit) {
				drawDef.overlay[drawDef.overlayLen++] = {
					.pos  = shared->hexes[i].pos,
					.type = DrawType_EnemyAttackable,
				};
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

static void ExecuteOrder(F32 sec) {
	OrderStep const* step = &order.steps[order.stepsIdx];
	order.elapsedSec += sec;
	if (order.elapsedSec >= step->durSec) {
		switch (step->type) {
			case OrderStepType::MoveThrough: {
				if (order.unit->move >= step->toHex->terrain->moveCost) {
					order.unit->move -= step->toHex->terrain->moveCost;
				} else {
					order.unit->move = 0;
				}
				break;
			}

			case OrderStepType::MoveOnto: {
				Hex* const oldHex = order.unit->hex;
				Hex* const newHex = step->toHex;
				Assert(!newHex->unit);
				oldHex->unit = nullptr;
				newHex->unit = order.unit;
				order.unit->hex = newHex;
				if (order.unit->move >= step->toHex->terrain->moveCost) {
					order.unit->move -= step->toHex->terrain->moveCost;
				} else {
					order.unit->move = 0;
				}
				break;
			}

			case OrderStepType::AttackIn: {
				Unit* const unit   = step->unit;
				F32   const yStart = unit->pos.y - unit->def->size.y / 2.f;
				Effect::CreateFloatingStr({
					.font   = numberFont,
					.str    = SPrintf(tempMem, "-1"),
					.durSec = 3.f,
					.x      = unit->pos.x,
					.yStart = yStart,
					.yEnd   = yStart - 20.f,	// TODO: make this configurable
				});
				if (unit->hp > 0) {
					unit->hp--;
				}
				if (unit->hp == 0) {
					order.steps[order.stepsLen++] = {
						.type   = OrderStepType::Die,
						.unit   = step->unit,
						.durSec = 1.f,	// TODO: configurable dur
					};
				}
				break;
			}

			case OrderStepType::AttackOut:
				order.unit->acted = true;
				order.unit->move = 0;
				break;

			case OrderStepType::Die: {
				Army* const army = &shared->armies[order.unit->side];
				U8 const unitIdx = (U8)(order.unit - army->units);
				*order.unit = army->units[--army->unitsLen];

				for (U16 i = 0; i < MaxHexes; i++) {
					army->attackMap[i] = Bit::MoveBit(army->attackMap[i], army->unitsLen, unitIdx);
				}
				break;
			}

			default:
				Panic("Unhandled OrderStepType %u", (U32)step->type);
		}

		order.elapsedSec -= step->durSec;	// may be > 0, this is fine
		step++;
		order.stepsIdx++;

		if (order.stepsIdx >= order.stepsLen) {
			BuildPathMap(shared->hexes, order.unit);
			BuildAttackMap(&shared->armies[order.unit->side]);
			if (!order.unit->acted) {
				// TODO: should we only select if the unit is in range of something?
				SelectOrderUnit(order.unit);
			} else {
				SelectNextUnit();
			}
			UpdateHover();
			RebuildOverlay();
			Logf("Order completed");
			return;
		}
	}

	F32 const t = order.elapsedSec / step->durSec;
	switch (step->type) {
		case OrderStepType::MoveThrough: [[fallthrough]];
		case OrderStepType::MoveOnto: {
			order.unit->pos = Math::Lerp(step->fromHex->pos, step->toHex->pos, t);
			break;
		}

		case OrderStepType::AttackIn: {
			Vec2 const pos = GetBorderPosBetween(order.unit->hex, step->unit->hex);
			order.unit->pos = Math::Lerp(order.unit->hex->pos, pos, t);
			break;
		}

		case OrderStepType::AttackOut: {
			Vec2 const pos = GetBorderPosBetween(order.unit->hex, step->unit->hex);
			order.unit->pos = Math::Lerp(pos, order.unit->hex->pos, t);
			break;
		}

		case OrderStepType::Die: {
			break;
		}

		default:
			Panic("Unhandled OrderStepType %u", (U32)step->type);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> Update(App::UpdateData const* updateData) {
	rebuildOverlay = false;

	InputResult inputResult; TryTo(HandleInput(updateData->sec, updateData->mouseX, updateData->mouseY, updateData->actions), inputResult);

	for (U64 i = 0; i < inputResult.clickHexes.len; i++) {
		inputResult.clickHexes[i] ? LeftClick(inputResult.clickHexes[i]) : RightClick();
	}
	if (showEnemyArmyThreatMap != inputResult.showEnemyArmyThreatMap) {
		rebuildOverlay = true;
	}
	showEnemyArmyThreatMap = inputResult.showEnemyArmyThreatMap;

	if (hoverHex != inputResult.hoverHex) {
		hoverHex = inputResult.hoverHex;
		UpdateHover();
	}

	if (state == State::OrderExecuting) {
		ExecuteOrder(updateData->sec);
	} else if (rebuildOverlay) {
		RebuildOverlay();
	}

	Effect::Update(updateData->sec);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

