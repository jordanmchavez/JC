#include "JC/Unit.h"

#include "JC/Array.h"
#include "JC/Draw.h"
#include "JC/Json.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefErr(Unit, Max);
DefErr(Unit, ResourceTypeNotFound);

//--------------------------------------------------------------------------------------------------

struct ResourceTypeDef {
	Str  name;
	Str  sprite;
	bool replenishesEachTurn;
};
Json_Begin(ResourceTypeDef)
	Json_Member("name",                name)
	Json_Member("sprite",              sprite)
	Json_Member("replenishesEachTurn", replenishesEachTurn)
Json_End(ResourceTypeDef)

struct DefenseTypeDef {
	Str name;
	Str sprite;
};
Json_Begin(DefenseTypeDef)
	Json_Member("name",   name)
	Json_Member("sprite", sprite)
Json_End(DefenseTypeDef)

struct DamageTypeDef {
	Str name;
	Str sprite;
	Str defenseType;
};
Json_Begin(DamageTypeDef)
	Json_Member("name",        name)
	Json_Member("sprite",      sprite)
	Json_Member("defenseType", defenseType)
Json_End(DamageTypeDef)

struct ResourceDef {
	Str name;
	U32 max;
};
Json_Begin(ResourceDef)
	Json_Member("name", name)
	Json_Member("max",  max)
Json_End(ResourceDef)

struct DefenseDef {
	Str name;
	U32 amount;
};
Json_Begin(DefenseDef)
	Json_Member("name",   name)
	Json_Member("amount", amount)
Json_End(DefenseDef)

struct AttackResourceCostDef {
	Str name;
	U32 amount;
};
Json_Begin(AttackResourceCostDef)
	Json_Member("name",   name)
	Json_Member("amount", amount)
Json_End(AttackResourceCostDef)

struct AttackDef {
	Str                         name;
	Str                         sprite;
	Str                         damageType;
	U32                         damage;
	U32                         counterDamage;
	Span<AttackResourceCostDef> resourceCosts;
};
Json_Begin(AttackDef)
	Json_Member("name",          name)
	Json_Member("sprite",        sprite)
	Json_Member("damageType",    damageType)
	Json_Member("damage",        damage)
	Json_Member("counterDamage", counterDamage)
	Json_Member("resourceCosts", resourceCosts)
Json_End(AttackDef)

struct UnitDef {
	Str               name;
	Str               sprite;
	Span<ResourceDef> resources;
	Span<DefenseDef>  defenses;
	Span<AttackDef>   attacks;
};
Json_Begin(UnitDef)
	Json_Member("name",      name)
	Json_Member("sprite",    sprite)
	Json_Member("resources", resources)
	Json_Member("defenses",  defenses)
	Json_Member("attacks",   attacks)
Json_End(UnitDef)

struct Def {
	Span<ResourceTypeDef> resourceTypes;
	Span<ResourceTypeDef> defenseTypes;
	Span<ResourceTypeDef> damageTypes;
	Span<UnitDef>         units;
};
Json_Begin(Def)
	Json_MemberOpt("resourceTypes", resourceTypes)
	Json_MemberOpt("defenseTypes",  defenseTypes)
	Json_MemberOpt("damageTypes",   damageTypes)
	Json_MemberOpt("units",         units)
Json_End(Def)

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxAttackResourceCosts = 32;
static constexpr U32 MaxUnitStats           = 32;
static constexpr U32 MaxUnitAttacks         = 32;

namespace StatFlags {
	constexpr U64 Resource            = (U64)1 << 0;
	constexpr U64 ReplenishesEachTurn = (U64)1 << 1;
};

// brace: 1 turn: +2 physical def and +2 counterattack damage
// level up: add points / wounding / debuff to attack
// Attack can be modified
// spells and suchcan't

struct Buff {
	U32 duration;
	Span<Modifier> modifiers;
};

struct StatType {
	Str          name;
	Draw::Sprite sprite;
	U64          flags;
};

enum struct SpellType {
	Invalid = 0,
	RangedProjectile,
	Buff,
};

struct Spell {
	SpellType type;
	U32 range;
};

enum struct AbilityType {
	Invalid = 0,
	Modifier,
	Aura,
	Spell,
};

// hp 1-25% -> -20% damage

enum struct ConditionType {
	Invalid = 0,
	StatPercentRange,
};

struct Condition {
	ConditionType type;
	StatType const* statType;
	U32 min;
	U32 max;
};

enum struct ModifierTarget {
	Invalid = 0,
	AttackDamagePercent,
};

enum struct ModifierSourceType {
	Invalid = 0,
	Ability,
	Buff,
	Item,
};

struct Modifier {
	Span<Condition> conditions;
	ModifierTarget target;
	I32 amount;
	Modifier* next;
	ModifierSourceType sourceType;
	union {
	} source;
};

struct Ability {
	Str             name;
	Draw::Sprite    sprite;
	AbilityType     type;
	Span<Modifier>  modifiers;
};

StatType healthStatType;

Ability foo = {
	.name = "WeakensAtLowHealth",
	.type = AbilityType::Modifier,
	.modifiers = {{
		.conditions = {{
			.type = ConditionType::StatPercentRange,
			.statType = &healthStatType,
			.min = 1,
			.max = 25,
		}},
		.target = ModifierTarget::AttackDamagePercent,
		.amount = -20,
	}},
};

struct Stat {
	StatType const* statType;
	U32             max;
	U32             cur;
	Modifier*       modifiers;
	Condition*      conditions;
};

struct DamageType {
	Str          name;
	Draw::Sprite sprite;
	StatType*    defensStatType;
};

struct ResourceCost {
	ResourceType const* resourceType;
	U32                 cost;
};

struct Attack {
	Str                                         name;
	Draw::Sprite                                sprite;
	DamageType const*                           damageType;
	U32                                         damage;
	U32                                         counterDamage;
	Array<ResourceCost, MaxAttackResourceCosts> resourceCosts;
};

struct Unit {
	Str                               name;
	Draw::Sprite                      sprite;
	Array<Stat, MaxUnitStats> stats;
	Array<Attack,   MaxUnitAttacks>   attacks;
};

static constexpr U32 Cfg_MaxResourceTypes = 32;
static constexpr U32 Cfg_MaxDefenseTypes  = 32;
static constexpr U32 Cfg_MaxDamageTypes   = 32;
static constexpr U32 Cfg_MaxUnits         = 1024;

//--------------------------------------------------------------------------------------------------

static Mem                  tempMem;
static MArray<ResourceType> resourceTypes;
static MArray<DefenseType>  defenseTypes;
static MArray<DamageType>   damageTypes;
static MArray<Unit>         units;

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMemIn) {
	tempMem = tempMemIn;

	resourceTypes.Init(permMem, Cfg_MaxResourceTypes); resourceTypes.Add();	// Reserve 0 for invalid
	defenseTypes.Init(permMem, Cfg_MaxDefenseTypes); defenseTypes.Add();	// Reserve 0 for invalid
	damageTypes.Init(permMem, Cfg_MaxDamageTypes); damageTypes.Add();	// Reserve 0 for invalid
	units.Init(permMem, Cfg_MaxUnits); units.Add();	// Reserve 0 for invalid

	return Ok();
}

//--------------------------------------------------------------------------------------------------

// TODO: user interned strings for 
static Res<ResourceType const*> FindResourceType(Str name) {
	for (U64 i = 0; i < resourceTypes.len; i++) {
		if (resourceTypes[i].name == name) {
			return &resourceTypes[i];
		}
	}
	return Err_ResourceTypeNotFound("name", name);
}

//--------------------------------------------------------------------------------------------------

Res<> Load(Str path) {
	ErrScope("path", path);

	Def def; Try(Json::Load(tempMem, path, &def));

	if (!resourceTypes.HasCapacity(def.resourceTypes.len)) { return Err_Max("type", "resourceTypes", "max", Cfg_MaxResourceTypes); }
	for (U64 i = 0; i < def.resourceTypes.len; i++) {
		ResourceType* resourceType = resourceTypes.Add();
		resourceType->name                = def.resourceTypes[i].name;
		resourceType->replenishesEachTurn = def.resourceTypes[i].replenishesEachTurn;
		TryTo(Draw::GetSprite(def.resourceTypes[i].sprite), resourceType->sprite);
	}

	if (!defenseTypes.HasCapacity(def.defenseTypes.len)) { return Err_Max("type", "defenseTypes", "max", Cfg_MaxDefenseTypes); }
	for (U64 i = 0; i < def.defenseTypes.len; i++) {
		DefenseType* defenseType = defenseTypes.Add();
		defenseType->name                = def.defenseTypes[i].name;
		TryTo(Draw::GetSprite(def.defenseTypes[i].sprite), defenseType->sprite);
	}

	if (!damageTypes.HasCapacity(def.damageTypes.len)) { return Err_Max("type", "damageTypes", "max", Cfg_MaxDamageTypes); }
	for (U64 i = 0; i < def.damageTypes.len; i++) {
		DamageType* damageType = damageTypes.Add();
		damageType->name                = def.damageTypes[i].name;
		TryTo(Draw::GetSprite(def.damageTypes[i].sprite), damageType->sprite);
	}

	if (!units.HasCapacity(def.units.len)) { return Err_Max("type", "units", "max", Cfg_MaxUnits); }
	for (U64 i = 0; i < def.units.len; i++) {
		UnitDef const* unitDef = &def.units[i];
		Unit* unit = units.Add();
		unit->name  = unitDef->name;
		TryTo(Draw::GetSprite(unitDef->sprite), unit->sprite);
		if (unitDef->resources.len >= MaxUnitResources) { return Err_Max("type", "unitResources", "max", MaxUnitResources); }
		for (U64 j = 0; j < unitDef->resources.len; j++) {
			Resource* resource = unit->resources.Add();
			resource->max      = unitDef->resources[j].max;
			resource->amount   = unitDef->resources[j].max;
			TryTo(FindResourceType(unitDef->resources[j].name), resource->resourceType);
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

ResourceType CreateResourceType(Str name, Draw::Sprite sprite);
ResourceType GetResourceType(Str name);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit