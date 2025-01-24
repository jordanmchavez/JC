#pragma once

#include "JC/Common.h"

namespace JC::Entity {

//--------------------------------------------------------------------------------------------------

constexpr u32 MaxQueryComponents = 32;

struct Entity    { u64 id; };
struct Component { u32 id; };
struct Query     { u32 id; };

struct Iter;	// Iter always hash temp arena scope

struct RowSet {
	Entity* entities;
	void*   componentData[MaxQueryComponents];
	u32     len;
};

void         Init(Arena* perm, Arena* temp);

Span<Entity> CreateEntities(u32 n, Span<Component> components);
void         DestroyEntities(Span<Entity> entitys);

Component    CreateComponent(s8 name, u32 len);
void         AddComponent(Entity entity, Component component);
void         RemoveComponent(Entity entity, Component component);
void*        ComponentData(Entity entity, Component component);

Query        CreateQuery(Span<Component> components);
Iter*        RunQuery(Query query);
RowSet*      Next(Iter* iter);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity
