#pragma once

#include "JC/Common.h"

namespace JC::Entity {

//--------------------------------------------------------------------------------------------------

using EntityId = u64;
using ComponentId = u32;

struct Iter;

void           Init(Arena* perm, Arena* temp);
Span<EntityId> CreateEntities(u32 n, Span<ComponentId> componentIds);
void           DestroyEntities(Span<EntityId> entityIds);
ComponentId    CreateComponent(s8 name, u32 len);
void           AddComponent(EntityId entityId, ComponentId componentId);
void           RemoveComponent(EntityId entityId, ComponentId componentId);
void*          ComponentData(EntityId entityId, ComponentId componentId);
Iter*          Query(Span<ComponentId> componentIds);
Span<void*>    Next(Iter* iter);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity
