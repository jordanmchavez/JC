- No unit selected
	- Arrow cursor
	- Hover -> border hex white, show unit+terrain stats
	- Click friendly hex -> unit selected
- Unit selected
	- Border reachable empty or friendly hexes in green
	- Border reachable enemy hexes in red
	- Hover reachable empty hex
		- Movement cursor
		- Show dotted path in shortest path hexes (incl hover hex)
	- Hover reachable friendly hex -> no path
	- Hover reachable enemy hex -> Attack cursor, show path
	- Hover unreachable hex -> X cursor

















# Battle Layer — Implementation Spec

## 1. Scope
This document fully specifies the battle layer as a standalone prototype module. The prototype uses hardcoded unit and army definitions; the player controls both sides. Graphics, sound, and AI are out of scope.

## 2. Grid
### 2.1 Geometry
- Hex grid, configurable dimensions, average map size 12x12.
- Coordinate system: **axial (q, r)**. Use the standard cube-coordinate distance formula: `(|dq| + |dr| + |ds|) / 2` where `s = -q - r`.
- Each hex has exactly 6 neighbors at unit distance.

### 2.2 Deployment Zones
- Side A deploys in the **first 2 columns** (q = 0, 1).
- Side B deploys in the **last 2 columns** (q = 10, 11 for a 12-wide grid).
- The number of available deployment columns is a general stat (Tactics) — use 2 columns per side for the prototype.
- One unit per hex.

## 3. Terrain
Each hex has exactly one terrain type. Terrain governs:
1. **Movement cost** — stamina per hex entered.
2. **Defense bonus** — modifier to the defender's DefSkl when attacked while on that hex.
3. **Ranged range bonus** — mountain hexes grant +1 to the short-range ceiling of ranged weapons (effectively extending short range by 1 hex; does not increase max range).
All values are data-defined.

### Default Terrain Table
| Terrain  | Move Cost | Defense Bonus |
|----------|-----------|---------------|
| Grass    | 1         | 0             |
| Hill     | 2         | +1 DefSkl     |
| Forest   | 2         | +1 DefSkl     |
| Mountain | 3         | +2 DefSkl     |
| Swamp    | 3         | −1 DefSkl     |

## 4. Unit Data
### 4.1 Static Unit Record
Defined once per unit type; shared across all instances.
```
struct Weapon {
	U32 atk;	// number of attacks
	U32 off;	// offense skill
	U32 str;	// strength
	U32 tgh;	// toughness
	U32 maxAmmo;
};

struct Ability {
	// TBD, abilities can pretty much affect any part of the game and can be active or passive
};

struct UnitType {
	Str name;
	U32 hp;
	U32 sta;	// max stamina
	U32 mor;	// max morale
	U32 mov;	// movement points per turn
	U32 def;	// defense skill
	U32 tgh;	// toughness
	U32 mana;
	bool hasZoc;
	Ability[] abilities
	Weapon[] weapons;
};

struct Unit {
	UnitType unitType;
	U32 curHp;
	U32 curSta;
	U32 curMor;
	Weapon curWeapon;	// currently equipped weapo
};
```

**Interval Definitions** — HP, stamina, and morale each carry their own threshold and penalty data (§4.3–§4.5).

### 4.2 Runtime Unit State

Per-unit state created fresh at battle start:

```
hpCurrent:              int
staminaCurrent:         int
manaCurrent:            int
moraleModifier:         int     // sum of accumulated in-battle morale modifiers (starts at 0)
equippedWeaponIndex:    int     // index into weapons list
hasActed:               bool    // true after unit attacks, rests, or uses an ending ability this turn
hasMoved:               bool    // true if unit moved ≥ 1 hex this activation
isRouting:              bool
isExhausted:            bool    // set at start of activation if staminaCurrent == 0
counterattackEnabled:   bool    // player-togglable
position:               HexCoord
side:                   Side    // A or B
```

### 4.3 HP Intervals

Thresholds and penalties are defined per unit type:

```
hpYellowThreshold:   int               // e.g., 50% of hpMax (absolute HP value)
hpRedThreshold:      int               // e.g., 25% of hpMax
hpYellowPenalties:   StatModifierList
hpRedPenalties:      StatModifierList
```

Suggested defaults: Yellow = `[{NumAttacks, −1}]`, Red = `[{NumAttacks, −2}]`.

When `hpCurrent` crosses a threshold (downward), two things happen immediately:
1. Apply the threshold's morale modifier (§9.5).
2. Trigger a morale check (§9.3).

Penalties apply passively as long as HP is in that band.

When `hpCurrent` reaches 0 the unit is **dead**: remove from battlefield. Dead units do not trigger morale events.

### 4.4 Stamina Intervals

```
staminaYellowThreshold:     int
staminaRedThreshold:        int
staminaYellowPenalties:     StatModifierList
staminaRedPenalties:        StatModifierList
staminaExhaustedPenalties:  StatModifierList   // applied while isExhausted == true
```

**Exhaustion:** A unit whose `staminaCurrent == 0` at the **start** of its activation is exhausted for that activation. It must rest (no movement, no attack) and suffers `staminaExhaustedPenalties` for the entire activation.

Stamina is clamped to `[0, staminaMax]` at all times.

### 4.5 Morale Intervals

**Effective morale** = `morale + moraleModifier`. Used for checks and interval evaluation.

```
moraleYellowThreshold:   int
moraleRedThreshold:      int
moraleYellowPenalties:   StatModifierList
moraleRedPenalties:      StatModifierList
```

Passive penalties apply as long as effective morale is in that band. Bonuses for high morale can be expressed as negative penalties on a "green bonus" band if desired (schema supports it; omit for prototype).

### 4.6 StatModifier Schema

A `StatModifier` is: `{ stat: StatName, delta: int }`.

`StatName` is one of: `NumAttacks`, `AtkSkl`, `DefSkl`, `Strength`, `Toughness`, `Movement`, `MoraleCheck` (added to effective morale for purposes of the check only).

A `StatModifierList` is a list of zero or more `StatModifier` records.

---

## 5. Weapons

Each weapon is a record on the unit:

```
name:           str
numAttacks:     int     // number of attack dice rolled
atkSkl:         int     // Attack Skill (added to hit roll)
strength:       int     // Strength (added to wound roll)
isRanged:       bool
maxRange:       int     // 0 for melee; in hexes
shortRangeMax:  int     // last hex considered short range (ignored if !isRanged)
ammoMax:        int     // 0 = unlimited
ammoCurrent:    int     // runtime state
```

### 5.1 Melee Weapons

`isRanged = false`, `maxRange = 0`. Can only target adjacent hexes. Melee attacks trigger a counterattack (§6.2).

### 5.2 Ranged Weapons

`isRanged = true`. Can target any hex within `maxRange` hexes. No line-of-sight blocking.

**Range bands:**
- **Short range:** 1–`shortRangeMax` hexes. No range penalty.
- **Long range:** `(shortRangeMax + 1)`–`maxRange` hexes. Apply −1 AtkSkl.

Mountain bonus: the effective `shortRangeMax` increases by 1 when the attacking unit is on a mountain hex.

**Move-and-shoot:** If the unit moved ≥ 1 hex before this ranged attack, apply −1 AtkSkl. Stacks with long-range penalty.

**Ammo:** Each ranged attack consumes 1 ammo. At ammo = 0 the weapon cannot be used.

Ranged attacks do **not** trigger counterattacks.

### 5.3 Weapon Switching

- The **equipped weapon** (`equippedWeaponIndex`) is used for attacks and counterattacks.
- Switching weapon is a free action, but only allowed **before the unit attacks** this activation.
- A unit can switch weapon at most once per activation.

---

## 6. Combat Resolution

### 6.1 Gathering Modifiers

Before rolling, collect all active `StatModifier` records that apply to the attacker and defender:
- HP interval penalties (§4.3)
- Stamina interval penalties (§4.4)
- Morale interval penalties (§4.5)
- Terrain defense bonus for the defender's hex (§3) — adds to DefSkl
- Flanking / pinching bonuses (§6.4)
- Range penalties (§5.2)
- Command action bonuses (§8.5)
- Any active ability modifiers

Sum deltas per stat. Apply before rolling.

### 6.2 Attack Roll

```
effectiveNumAttacks = max(1, weapon.numAttacks + Σ(NumAttacks modifiers))
effectiveAtkSkl     = weapon.atkSkl     + Σ(AtkSkl modifiers on attacker)
effectiveDefSkl     = defender.defSkl   + Σ(DefSkl modifiers on defender)
effectiveStr        = weapon.strength   + Σ(Strength modifiers on attacker)
effectiveTgh        = defender.toughness + Σ(Toughness modifiers on defender)

wounds = 0
for i in 1..effectiveNumAttacks:
    if (roll_d10() + effectiveAtkSkl - effectiveDefSkl) >= 6:   // hit
        if (roll_d10() + effectiveStr - effectiveTgh) >= 6:     // wound
            wounds += 1

defender.hpCurrent -= wounds
```

Check HP thresholds after applying damage (§4.3).

### 6.3 Counterattack

After a **melee** attack resolves, the defender automatically counterattacks if all are true:
- `defender.counterattackEnabled == true`
- `defender.staminaCurrent > 0`
- Attacker is within range of the defender's equipped weapon (typically melee = adjacent)

Resolve counterattack using §6.2 with the defender as attacker. Costs the defender 1 stamina. Does **not** consume the defender's action for their own upcoming activation. Does **not** trigger a further counterattack from the original attacker. Does **not** apply the move+attack stamina surcharge regardless of the defender's movement earlier.

### 6.4 Flanking and Pinching

Evaluate relative to the **defender's hex** at the moment of attack. Count hexes adjacent to the defender that contain an attacking-side unit (including the attacker).

**Pinched** — two of those hexes are on **exactly opposite sides** of the defender (hex directions that sum to zero in cube coordinates).
- Bonus: +1 AtkSkl and +1 Strength to all attackers participating in this attack.

**Flanked** — Three units form a "triangle" around the defender (so three attackers adjacent and spaced one hex apart. Note that if a a 4th/5th/6th unit is added to the mix, the unit is still flanked since there "exists" a flanking triangle. Though in thie case the unit will also necessarily be Pinched.
- Bonus: +1 AtkSkl to all attackers participating in this attack.

Units have a bool flag indicating whether they can contribute to flanking/pinching (for example, archers shouldn't flank or pinch).
Pinched overrides Flanked if both conditions are met.

> Design note: Pinch (2 units, opposite sides) currently gives a larger bonus than Flank (3+ units, spread). Consider swapping bonus values if playtesting finds the easier condition over-rewarded.

### 6.5 Attack of Opportunity

Triggered when a unit moves from a hex inside an enemy's ZoC to a hex **not** in that enemy's ZoC (including routing movement).

The enemy immediately makes one free attack against the departing unit:
- Uses enemy's equipped weapon; resolved via §6.2.
- Costs 1 stamina from the opportunity attacker (if attacker has 0 stamina, no AoO occurs).
- Does **not** consume the opportunity attacker's action for their activation.
- Does **not** trigger a counterattack from the departing unit.
- Check HP thresholds on the departing unit after damage.

---

## 7. Zone of Control

A unit with `hasZoC = true` exerts ZoC over all 6 adjacent hexes.

**Movement rules — evaluated hex by hex as the unit moves:**

1. **Entering ZoC from outside:** Moving from a hex not in enemy X's ZoC into a hex in enemy X's ZoC **ends movement** for this activation. The unit stops in that hex.
2. **Moving within ZoC of the same enemy:** Moving from a hex in enemy X's ZoC to another adjacent hex also in enemy X's ZoC is permitted. (The unit is already engaged; shifting position is allowed.) This still costs the normal stamina for that hex.
3. **Exiting ZoC:** Moving from a hex in enemy X's ZoC to a hex not in enemy X's ZoC triggers an **AoO** from enemy X (§6.5).
4. **Multiple ZoC sources:** Each enemy's ZoC is evaluated independently. Entering hex H that is in ZoC of both X and Y stops movement (rule 1). Exiting X's ZoC while staying in Y's ZoC triggers AoO from X only.

**Routing units** obey ZoC exit AoO but ignore the movement-stop rule when entering ZoC (they flee regardless).

---

## 8. Turn Structure

### 8.1 Battle Round

1. Determine first player (coin flip for prototype; future: general's Tactics stat).
2. **Active side's phase:**
   a. Use command actions at any point during the phase (§8.5).
   b. Activate each unit on the active side exactly once, in any order chosen by the player.
3. **Inactive side's phase:** Same.
4. Check win conditions (§10).
5. Repeat.

### 8.2 Unit Activation

A unit's activation proceeds in this order:

1. **Exhaustion check:** If `staminaCurrent == 0`, set `isExhausted = true`. The unit must Rest this activation (skip to step 3, Rest only). Apply `staminaExhaustedPenalties` for the activation.

2. **Move** (optional): Move up to `movement` hexes, paying 1 stamina per hex. Obeys ZoC rules (§7). Terrain movement costs replace the base 1 stamina (e.g., entering a hill hex costs 2 stamina). Set `hasMoved = true` if any hexes are traversed. Movement stops immediately on entering a ZoC hex (§7, rule 1).

3. **Action** (choose one — unless exhausted, in which case Rest is mandatory):

   | Action       | Description                                                      | Ends Activation? |
   |--------------|------------------------------------------------------------------|------------------|
   | **Attack**   | Attack a target in range with equipped weapon (§6.2).            | Yes              |
   | **Rest**     | Recover 2 stamina. Apply +1 morale modifier.                     | Yes              |
   | **Ability**  | Activate an active ability. Ending behavior defined per ability. | Varies           |
   | **Pass**     | End activation immediately.                                      | Yes              |

   Attacking also sets `hasActed = true`. After attacking, the unit **cannot move further** unless it has an ability explicitly granting post-attack movement.

4. **Weapon switch** (optional, only if `hasActed == false`): Change `equippedWeaponIndex`.

### 8.3 Stamina Costs

| Action                                    | Stamina Cost |
|-------------------------------------------|--------------|
| Move 1 hex                                | 1 (or terrain cost) |
| Attack                                    | 1            |
| Surcharge: moved ≥ 1 hex and attacked     | +1           |
| Counterattack                             | 1            |
| Rest                                      | — (restores 2) |
| Attack of opportunity (opportunity attacker) | 1         |

The surcharge applies if `hasMoved == true` when the attack action is taken. Stamina is clamped to `[0, staminaMax]`.

### 8.4 Routing Units

On activation, a routing unit automatically moves toward the nearest table edge and away from enemy units. The exact algorithm is TBD and expected to be iterated on; one approach: pick the adjacent hex that maximizes distance from nearest enemy while minimizing distance to nearest edge. Routing movement uses the unit's full `movement` stat, obeys terrain costs, and triggers AoO on ZoC exits (§6.5). The controlling player cannot issue orders to a routing unit.

If a routing unit reaches the table edge, it is **removed from battle** (survived/fled, not dead). Routing units still on the table do not count as eliminated for win-condition purposes.

### 8.5 Command Actions

Each side has a **command pool** of actions per turn, granted by the general. Default pool: **2 per turn**. Pool resets at the start of each round. May be used at any point during the active side's phase.

| Command Action  | Cost | Effect                                                         |
|-----------------|------|----------------------------------------------------------------|
| Rally           | 1    | End one routing unit's rout; set `isRouting = false`, reset `moraleModifier` to 0. |
| Enhance Attack  | 1    | Grant +2 AtkSkl to one unit's next attack this turn.          |
| Grant Movement  | 1    | Grant one unit +2 movement this turn.                         |
| Heal            | 1    | Restore 1 HP to a unit (does not un-cross HP thresholds).     |

---

## 9. Morale

### 9.1 Effective Morale

`effectiveMorale = morale + moraleModifier`

`moraleModifier` accumulates from events during the battle (can be negative or positive). See §9.5.

### 9.2 Morale Intervals

Passive combat penalties apply when `effectiveMorale` falls into yellow or red bands (data-defined per unit type, see §4.5).

### 9.3 Morale Check

A check is triggered by:
- HP crosses into yellow threshold (downward).
- HP crosses into red threshold (downward).
- Other effects like terrifying monsters or fear spells.

**Check resolution:** Roll **3d10**. If `result < effectiveMorale` → unit **routs** (`isRouting = true`). Otherwise no effect.

### 9.4 Routing

See §8.4 for routing movement. A routing unit can be recovered via the Rally command action (§8.5).

### 9.5 Morale Modifier Events

| Event                                          | `moraleModifier` delta |
|------------------------------------------------|------------------------|
| Friendly unit killed within 3 hexes           | −2                     |
| Unit's HP crosses into yellow threshold        | −1                     |
| Unit's HP crosses into red threshold           | −3                     |
| Unit rests                                     | +1                     |
| Unit kills an enemy                            | +1                     |

All values are defaults; tune per unit type or ability as needed. Modifiers stack and persist for the remainder of the battle unless cleared (e.g., by Rally resetting `moraleModifier` to 0).

---

## 10. Win Conditions

The battle ends at the end of a round after checking:

- **Side X wins** if all of Side Y's units are dead or have fled off the table edge. Routing units still on the table do not count.
- **Draw** — not expected but handle if both sides simultaneously reach zero units.
- **Retreat** — a side may voluntarily move all of its units off the table edge during its phase. Retreating units are exposed to AoO when exiting ZoC hexes. Units that successfully exit survive.

In the prototype (player controls both sides), an explicit "Concede" action for either side ends the battle immediately.

---

## 11. Prototype Scope

### 11.1 Hardcoded Armies

Define 2 armies, each with 4–6 units as static data. Choose unit types to exercise core mechanics:

| Archetype | ZoC | Ranged | Notes                                 |
|-----------|-----|--------|---------------------------------------|
| Infantry  | Yes | No     | Baseline melee unit                   |
| Archer    | No  | Yes    | Covers ranged, ammo, range bands      |
| Heavy     | Yes | No     | High HP/toughness, slow movement      |
| Caster    | No  | Yes    | Mana pool; placeholder for spells     |

### 11.2 Feature Checklist

Must implement for meaningful playtesting:

- [ ] Axial hex grid with terrain types
- [ ] Deployment phase (manual hex selection)
- [ ] Unit activation loop (move, attack, rest, pass)
- [ ] Stamina tracking, intervals, exhaustion
- [ ] HP tracking, intervals, death
- [ ] Morale tracking, modifier accumulation, checks, routing
- [ ] Melee combat resolution (§6.2)
- [ ] Ranged combat resolution (§5.2, §6.2)
- [ ] Counterattack (§6.3)
- [ ] Zone of Control (§7)
- [ ] AoO on ZoC exit (§6.5)
- [ ] Flanking / pinching detection (§6.4)
- [ ] Command action pool (§8.5)
- [ ] Win condition detection (§10)
- [ ] Weapon switching (§5.3)

Defer to later iteration:

- AI opponent
- General as a battlefield unit (command pool is present but sourced from a fixed value)
- Active spell system (mana data present; no spell effects implemented)
- Full active ability system
- Tactics-stat-based deployment
- Procedural map generation
- Unit XP / leveling
