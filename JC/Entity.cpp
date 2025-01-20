#include "JC/Entity.h"

namespace JC::Entity {

//--------------------------------------------------------------------------------------------------

u64* entityFree;
u64* entityBlock;
u64* entityBlockEnd;

struct Component {
	u32 id;
	u64 size;
	u64 align;
};

constexpr u32 MaxComponents = 1024;
Component components[MaxComponents];
u32 componentsLen;

constexpr u32 MaxTypeComponents = 64;
struct Type {
	u32 len;
	u32* entities;
	Component* components[MaxTypeComponents];
	void* componentData[MaxTypeComponents];
};

constexpr u32 MaxTypes = 1024;
Type types[MaxTypes];
u32 typesLen;

u64 CreateEntity() {
	if (entityFree) {
		u64 e = *entityFree;
		entityFree = *entityFree;
	}
}

void CreateComponent(u64 size, u64 align) {
};

void AddComponent(World* world, u64 e, 

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity
