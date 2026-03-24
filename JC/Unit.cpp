#include "JC/Unit.h"

#include "JC/Array.h"
#include "JC/Draw.h"
#include "JC/Json.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

DefErr(Unit, MaxResourceTypes);
DefErr(Unit, MaxDefenseTypes);
DefErr(Unit, MaxDamageTypes);
DefErr(Unit, MaxDamageAbilities);

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

struct UnitResourceDef {
	Str name;
	U32 max;
};
Json_Begin(UnitResourceDef)
	Json_Member("name", name)
	Json_Member("max",  max)
Json_End(UnitResourceDef)

struct UnitDefenseDef {
	Str name;
	U32 amount;
};
Json_Begin(UnitDefenseDef)
	Json_Member("name",   name)
	Json_Member("amount", amount)
Json_End(UnitDefenseDef)

struct ResourceCostDef {
	Str name;
	U32 amount;
};
Json_Begin(ResourceCostDef)
	Json_Member("name",   name)
	Json_Member("amount", amount)
Json_End(ResourceCostDef)

struct MeleeAttackDef {
	Str dxamageType;
	U32 damage;

};
Json_Begin(MeleeAttackDef)
Json_End(MeleeAttackDef)

struct RangedAttackDef {
};
Json_Begin(RangedAttackDef)
Json_End(RangedAttackDef)

struct UnitDef {
	Str name;
	Str sprite;
	U32 health;
	U32 movement;
	Span<UnitResourceDef> resources;
	Span<UnitDefenseDef> defenses;
	Span<UnitAbilityDef> abilities;
};
Json_Begin(UnitDef)
Json_End(UnitDef)

struct Def {
	Span<ResourceTypeDef> resourceTypes;
	Span<ResourceTypeDef> defenseTypes;
	Span<ResourceTypeDef> damageTypes;
	Span<AbilityDef>      abilities;
};
Json_Begin(Def)
	Json_MemberOpt("resourceTypes", resourceTypes)
	Json_MemberOpt("defenseTypes",  defenseTypes)
	Json_MemberOpt("damageTypes",   damageTypes)
	Json_MemberOpt("abilities",     abilities)
Json_End(Def)

//--------------------------------------------------------------------------------------------------

struct ResourceType {
	Str          name;
	Draw::Sprite sprite;
	bool         replenishesEachTurn;
};

struct DefenseType {
	Str          name;
	Draw::Sprite sprite;
};

struct DamageType {
	Str                name;
	Draw::Sprite       sprite;
	DefenseType const* defenseType;
};

enum struct ConditionType {
	Invalid = 0,
	ResourcePercentRange,
};


enum struct ModifierType {
	Invalid = 0,
	DamagePercent,
	DefensePercent,
};

//--------------------------------------------------------------------------------------------------

static constexpr U32 Cfg_MaxResourceTypes = 32;
static constexpr U32 Cfg_MaxDefenseTypes  = 32;
static constexpr U32 Cfg_MaxDamageTypes   = 32;
static constexpr U32 Cfg_MaxAbilities     = 1024;

static Mem                 tempMem;
static Array<ResourceType> resourceTypes;
static ResourceType        healthResourceType;
static ResourceType        movementResourceType;
static Array<DefenseType>  defenseTypes;
static Array<DamageType>   damageTypes;

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMemIn) {
	tempMem = tempMemIn;

	resourceTypes.Init(permMem, Cfg_MaxResourceTypes);
	resourceTypes.Add();	// Reserve 0 for invalid
	defenseTypes.Init(permMem, Cfg_MaxDefenseTypes);
	defenseTypes.Add();	// Reserve 0 for invalid
	damageTypes.Init(permMem, Cfg_MaxDamageTypes);
	damageTypes.Add();	// Reserve 0 for invalid


	Draw::Sprite healthSprite; TryTo(Draw::GetSprite("Icon_Health"), healthSprite);
	healthResourceType = CreateResourceType("Health", healthSprite);

	Draw::Sprite movementSprite; TryTo(Draw::GetSprite("Icon_Movement"), movementSprite);
	movementResourceType = CreateResourceType("Movement", movementSprite);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Load(Str path) {
	Def def; Try(Json::Load(tempMem, path, &def));

	if (!resourceTypes.HasCapacity(def.resourceTypes.len)) { return Err_MaxResourceTypes(); }
	for (U64 i = 0; i < def.resourceTypes.len; i++) {
		ResourceType* resourceType = resourceTypes.Add();
		resourceType->name                = def.resourceTypes[i].name;
		resourceType->replenishesEachTurn = def.resourceTypes[i].replenishesEachTurn;
		TryTo(Draw::GetSprite(def.resourceTypes[i].sprite), resourceType->sprite);
	}

	if (!defenseTypes.HasCapacity(def.defenseTypes.len)) { return Err_MaxDefenseTypes(); }
	for (U64 i = 0; i < def.defenseTypes.len; i++) {
		DefenseType* defenseType = defenseTypes.Add();
		defenseType->name                = def.defenseTypes[i].name;
		TryTo(Draw::GetSprite(def.defenseTypes[i].sprite), defenseType->sprite);
	}

	if (!damageTypes.HasCapacity(def.damageTypes.len)) { return Err_MaxDamageTypes(); }
	for (U64 i = 0; i < def.damageTypes.len; i++) {
		DamageType* damageType = damageTypes.Add();
		damageType->name                = def.damageTypes[i].name;
		TryTo(Draw::GetSprite(def.damageTypes[i].sprite), damageType->sprite);
	}

	if (!abilities.HasCapacity(def.abilities.len)) { return Err_MaxAbilities(); }
	for (U64 i = 0; i < def.abilities.len; i++) {
		DamageType* damageType = abilities.Add();
		damageType->name                = def.abilities[i].name;
		TryTo(Draw::GetSprite(def.abilities[i].sprite), damageType->sprite);
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

ResourceType CreateResourceType(Str name, Draw::Sprite sprite);
ResourceType GetResourceType(Str name);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit