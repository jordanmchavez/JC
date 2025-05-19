#pragma once

#include "JC/Core.h"

namespace JC::Entity {

//--------------------------------------------------------------------------------------------------

constexpr U32 MaxQueryComponents = 32;

struct Entity    { U64 id; };
struct Component { U32 id; };
struct Query     { U32 id; };

struct Iter;

struct RowSet {
	Entity* entities;
	void*   componentData[MaxQueryComponents];
	U32     len;
};

void         Init(Mem::Allocator* allocator, Mem::TempAllocator* tempAllocator);

Component    CreateComponent(Str name, U32 size);

Span<Entity> CreateEntities(U32 n, Span<Component> components);
void         DestroyEntities(Span<Entity> entitys);

void         AddComponents(Entity entity, Span<Component> components);
void         RemoveComponents(Entity entity, Span<Component> components);
Span<void*>  GetComponentData(Entity entity, Span<Component> components);

Query        CreateQuery(Span<Component> components);
Iter*        RunQuery(Query query);	// Iter always hash temp arena scope
RowSet*      Next(Iter* iter);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity
