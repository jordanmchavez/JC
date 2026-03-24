#include "JC/Unit.h"

#include "JC/Array.h"
#include "JC/Draw.h"
#include "JC/Json.h"

namespace JC::Unit {

//--------------------------------------------------------------------------------------------------

Err_Def(Unit, Max);
Err_Def(Unit, ResourceTypeNotFound);

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

static constexpr U32 MaxUnitResources  = 32;
static constexpr U32 MaxUnitDefenses   = 32;
static constexpr U32 MaxUnitAttacks    = 32;

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

struct Resource {
	ResourceType const* resourceType;
	U32                 amount;
	U32                 max;
};

struct Defense {
	DefenseType const* defenseType;
	U32                amount;
};

struct UnitAttack {

};

struct Unit {
	Str                                 name;
	Draw::Sprite                        sprite;
	Array<Resource,   MaxUnitResources> resources;
	Array<Defense,    MaxUnitDefenses>  defenses;
	Array<UnitAttack, MaxUnitAttacks>   attacks;
};

//--------------------------------------------------------------------------------------------------

static constexpr U32 Cfg_MaxResourceTypes = 32;
static constexpr U32 Cfg_MaxDefenseTypes  = 32;
static constexpr U32 Cfg_MaxDamageTypes   = 32;
static constexpr U32 Cfg_MaxUnits         = 1024;

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
	Err_Scope("path", path);

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
				TryTo(FindResourceType(unitDef->resources[j].name), resource->resourceType);
				.max= unitDef->resources[j].
				.amount = unitDef->resources[j].
			});
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

ResourceType CreateResourceType(Str name, Draw::Sprite sprite);
ResourceType GetResourceType(Str name);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unit