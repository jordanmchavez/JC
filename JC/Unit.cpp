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