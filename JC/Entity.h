#pragma once

#include "JC/Core.h"

namespace JC::Entity {

//--------------------------------------------------------------------------------------------------

constexpr u32 MaxQueryComponents = 32;

struct Entity    { u64 id; };
struct Component { u32 id; };
struct Query     { u32 id; };

struct Iter;

struct RowSet {
	Entity* entities;
	void*   componentData[MaxQueryComponents];
	u32     len;
};

void         Init(Mem::Allocator* allocator, Mem::TempAllocator* tempAllocator);

Component    CreateComponent(Str name, u32 size);

Span<Entity> CreateEntities(u32 n, Span<Component> components);
void         DestroyEntities(Span<Entity> entitys);

void         AddComponents(Entity entity, Span<Component> components);
void         RemoveComponents(Entity entity, Span<Component> components);
Span<void*>  GetComponentData(Entity entity, Span<Component> components);

Query        CreateQuery(Span<Component> components);
Iter*        RunQuery(Query query);	// Iter always hash temp arena scope
RowSet*      Next(Iter* iter);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity
