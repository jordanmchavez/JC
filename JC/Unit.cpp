#include "JC/Unit.h"

#include "JC/Draw.h"
#include "JC/Def.h"
#include "JC/Json.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxResourceTypes = 1024;

struct UnitResourceDef {
	Str          name;
	Draw::Sprite sprite;
	bool         replenishesEachTurn;
};
Json_Begin(UnitResourceDef)
	Json_Member("name",                name)
	Json_Member("sprite",              sprite)
	Json_Member("replenishesEachTurn", replenishesEachTurn)
Json_End(UnitResourceDef)

//--------------------------------------------------------------------------------------------------


struct ResourceTypeObj {
	Str          name;
	Draw::Sprite sprite;
};

static ResourceTypeObj* resourceTypeObjs;
static U32              resourceTypeObjsLen;
static ResourceType     healthResourceType;
static ResourceType     movementResourceType;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem) {
	resourceTypeObjs    = Mem::AllocT<ResourceTypeObj>(permMem, MaxResourceTypes);
	resourceTypeObjsLen = 1;	// Reserve 0 for invalid

	Def::RegisterArray<UnitResourceDef>("unitResources", MaxResourceTypes);
}

//--------------------------------------------------------------------------------------------------

Res<> InitDefs() {
	Draw::Sprite healthSprite; TryTo(Draw::GetSprite(Def::GetStr(Def::UnitHealthResourceSprite)), healthSprite);
	healthResourceType = CreateResourceType("Health", healthSprite);

	Draw::Sprite movementSprite; TryTo(Draw::GetSprite(Def::GetStr(Def::UnitMovementResourceSprite)), movementSprite);
	movementResourceType = CreateResourceType("Movement", movementSprite);

	Def::GetUnitResources
}

//--------------------------------------------------------------------------------------------------

ResourceType CreateResourceType(Str name, Draw::Sprite sprite);
ResourceType GetResourceType(Str name);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit

/*
---BEGIN CLAUDE---

Design discussion: Def system architecture (data-driven def loading).

The Def system loads all .json5 def files on startup. Top-level arrays are aggregated across files.
Each subsystem registers its schema during Init(), the Def system loads all files, then each subsystem
ingests the loaded data via InitDefs().

Example:
	// HumanUnits.json5 and OrcUnits.json5 both have `units: [...]`
	// After loading, there is one aggregated `units` array for Unit subsystem to ingest.

--- Q: Architecture review. What am I not thinking of? ---

[CLAUDE] Dangling strings/spans after freeing the Def arena — biggest landmine. Str members in loaded
def structs point into arena memory. Freeing it makes every string dangle instantly.

[JORDAN] Non-issue. All strings are interned via StrDb::Intern. Json layer already does this.
The def structs (e.g. UnitResourceDef) are intermediate parse targets, not game structs.
Everything gets converted during InitDefs(). If a def struct happens to be the same as a game
struct, the strings are already interned so still safe.

[CLAUDE] Type safety gap: GetArray<T> is templated but the registry is type-erased. A mismatched
T gives silent memory reinterpretation.

[JORDAN] RegisterArray returns a typed handle. GetArray takes the handle and Panics on mismatch.
Type uniqueness via: `template <class T> struct TypeId { inline static char Ptr = 0; };`
-> use `&TypeId<T>::Ptr` as a unique per-type pointer. (Note: needs `inline static` to avoid
requiring a separate .cpp definition.)

[CLAUDE] maxLen — who owns it, what happens on overflow?

[JORDAN] Owned by the subsystem doing Register/Get. Exceed maxLen = Load() returns a failed Res<>
that bubbles up to App::Run, program terminates with error.

[CLAUDE] The `Def::GetStr(Def::UnitHealthResourceSprite)` pattern in Unit.cpp needs designing —
appears to be a named scalar concept separate from the array aggregation system.

[JORDAN] Unit.cpp was wrong, earlier design. Will now be:
	handle = Def::RegisterObj<Str>("unitHealthResourceSprite")
	Def::Get<Str>(handle)
Consistent with the array handle pattern.

[CLAUDE] What happens when a key in a JSON file has no registered type?

[JORDAN] No registration = error, program terminates. No silent skipping, no guessing.
Implicit constraint: all Init() calls (which do Register*) must complete before the first Load()
call, otherwise valid keys could false-error. Enforce with state assertions in Register/Load.

[CLAUDE] Cross-def references and InitDefs ordering — nothing enforces that SpellDefs are
initialized before UnitDefs if units reference spells by name.

[JORDAN] InitDefs order is by-subsystem, explicit in App::Run. E.g. Draw::InitDefs() before
Unit::InitDefs() since units depend on draw sprites. Could make this more explicit by keying
registrations to subsystem name: Def::Register(Str subsystem, Str name, ...) with documented
subsystem load order so modders can reason about dependencies.

[CLAUDE] Good — subsystem name in Register pays for itself in error messages even if not used
for ordering: "duplicate key 'units' registered by subsystem 'Unit'" vs "duplicate key 'units'".

[JORDAN] Agreed, adding subsystem name.

[CLAUDE] Batch conflict model (latest-wins per batch, error intra-batch) doesn't map cleanly
to arrays since there's no natural key — array aggregation is always additive.

[JORDAN] Correct. Arrays are always additive. For objects, error on duplicate definitions.
Subsystems handle array-level duplicate logic themselves (e.g. Draw errors if same sprite is
defined twice, Unit errors if same resource type is defined twice, etc.). No batch system needed.

--- Three-phase init flow (settled design) ---

1. Init()      — all subsystems. All Register* calls happen here.
2. Def::Load() — all .json5 files. All registered keys must be present; unknown keys error.
3. InitDefs()  — all subsystems in dependency order (explicit in App::Run).

The Def arena can be safely freed after the last InitDefs() returns. Def structs are intermediate
parse targets that are fully consumed and converted to game structs during InitDefs(). All strings
live in StrDb, not the Def arena.

---END CLAUDE---
*/