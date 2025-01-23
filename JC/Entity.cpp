#include "JC/Entity.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Hash.h"
#include "JC/Map.h"

namespace JC::Entity {

//--------------------------------------------------------------------------------------------------

static constexpr u32 MaxComponents   = 1024;
static constexpr u32 TypeIdShift     = 10;
static constexpr u32 MaxTypes        = 1 << TypeIdShift;
static constexpr u32 MaxTypeColumns  = 64;
static constexpr u64 TypeDataReserve = 1024 * 1024 * 1024;
static constexpr u32 TypeInitialCap  = 16;

using TypeId = u32;

struct ComponentMask {
	u64 bits[MaxComponents / 64];
};

struct TypeMask {
	u64 bits[MaxTypes / 64];
};

struct Component {
	s8  name;
	u32 size;
};

struct Column {
	u32 componentId;
	u32 offset;
	u32 size;
};

struct Type {
	ComponentMask componentMask;
	u32           rowSize;
	Column        columns[MaxTypeColumns];	// TODO: we could very easily make this dynamic to remove the limit
	u32           columnsLen;
	Arena         arena;
	u8*           data;
	u32           len;
	u32           cap;
};

struct Entity {
	union {
		u32 typeId;
		u32 nextFreeIdx;
	};
	u32 gen;
	u32 row;
};

static Arena*                       perm;
static Arena*                       temp;
static Arena                        entitiesArena;
static Array<Entity>                entities;
static u32                          freeEntityIdx;
static Component                    components[MaxComponents];
static u32                          componentsLen;
static Type                         types[MaxTypes];
static u32                          typesLen;
static Map<u32, u32>                typeIdComponentIdToColumn;	// key = typeId | (componentId << TypeIdShift)
static Map<ComponentMask, TypeId>   componentMaskToTypeId;
static Map<ComponentMask, TypeMask> componentMaskToTypeMask;

//--------------------------------------------------------------------------------------------------

void Init(Arena* permIn, Arena* tempIn) {
	perm = permIn;
	temp = tempIn;
	page arena;
	entities;
	initial empty/zero type;
}

//--------------------------------------------------------------------------------------------------

static ComponentMask BuildComponentMask(Span<ComponentId> componentIds) {
	ComponentMask mask = {};
	for (u64 i = 0; i < componentIds.len; i++) {
		const ComponentId componentId = componentIds[i];
		mask.bits[componentId / 64] |= (u64)1 << (componentId & 63);
	}
	return mask;
}

//--------------------------------------------------------------------------------------------------

static TypeId CreateType(const ComponentMask* componentMask) {
	Assert(typesLen < MaxTypes);
	u32 typeId = typesLen;
	Type* type = &types[typesLen++];
	*type = {};

	// Iterate over each mask component
	for (u32 i = 0; i < MaxComponents / 64; i++) {
		u32 bits = componentMask->bits[i];
		if (!bits) { continue; }

		while (const u32 bit = Bsf64(bits)) {
			bits &= ~((u64)1 << bit);

			u32 componentId = (i * 64) + bit;
			typeIdComponentIdToColumn.Put(
				typeId | (componentId << TypeIdShift),
				type->columnsLen
			);

			Component* component = &components[componentId];
			type->columns[type->columnsLen++] = {
				.componentId = componentId,
				.offset      = type->rowSize,
				.size        = component->size,
			};

			type->rowSize += component->size;
		}
	}

	type->componentMask = *componentMask;
	type->arena         = CreateArena(TypeDataReserve);
	type->data          = (u8*)type->arena.Alloc(TypeInitialCap * type->rowSize);

	for (u32 i = 0; i < componentMaskToTypeMask.elemsLen; i++) {
		if (Contains(componentMask, &componentMaskToTypeMask.elems[i].key)) {
			componentMaskToTypeMask.elems[i].val.bits[typeId / 64] |= (u64)1 << typeId & 63;
		}
	}

	return typeId;
}

//--------------------------------------------------------------------------------------------------

Span<u32> AddRows(Type* type, u32 n) {
	const u32 oldLen = type->len;
	const u32 newLen = type->len + n;
	if (newLen > type->cap) {
		const u32 newCap = Max(newLen, type->cap * 2);
		Assert(type->arena.Extend(type->data, type->cap * type->rowSize, newCap * type->rowSize));
		
		// Don't need to move the first one
		for (u32 i = 1; i < type->columnsLen; i++) {
			const Column* const col = &type->columns[i];
			memmove(
				type->data + (type->cap * col->offset),
				type->data + (newCap    * col->offset),
				type->len * col->size
			);
		}

		type->cap = newCap;
	}
	type->len = newLen;

	Array<u32> rows(temp, n);
	u32 row = oldLen;
	for (u32 i = 0; i < n; i++) {
		rows[i] = row++;
	}

	return rows;
}

//--------------------------------------------------------------------------------------------------

void RemoveRow(Type* type, u32 row) {
	const u32 srcRow = type->len - 1;
	for (u32 c = 0; c < type->columnsLen; c++) {
		Column* col = &type->columns[c];
		void* dst = type->data + (col->offset * type->cap) + (row    * col->size);
		void* src = type->data + (col->offset * type->cap) + (srcRow * col->size);
		memcpy(dst, src, col->size);
	}
	type->len--;
}

//--------------------------------------------------------------------------------------------------

u32 CopyRow(Type* srcType, u32 srcRow, Type* dstType, u32 dstRow) {
	u32 srcColIdx = 0;
	u32 dstColIdx = 0;
	while (srcColIdx < srcType->columnsLen && dstColIdx < dstType->columnsLen) {
		const Column* const srcCol = &srcType->columns[srcColIdx];
		const Column* const dstCol = &dstType->columns[dstColIdx];
		if (srcCol->componentId == dstCol->componentId) {
			memcpy(
				srcType->data + (srcCol->offset * srcType->cap) + (srcRow * srcCol->size),
			    dstType->data + (dstCol->offset * dstType->cap) + (dstRow * dstCol->size),
				components[srcCol->componentId].size
			);
			srcColIdx++;
			dstColIdx++;
		} else if (srcCol->componentId < dstCol->componentId) {
			srcColIdx++;
		} else {
			dstColIdx++;
		}
	}
}

//--------------------------------------------------------------------------------------------------

Span<EntityId> CreateEntities(u32 n, Span<ComponentId> componentIds) {
	Array<EntityId> entityIds(temp, n);

	const ComponentMask mask = BuildComponentMask(componentIds);
	const TypeId* const typeIdPtr = componentMaskToTypeId.Find(mask);
	TypeId typeId = typeIdPtr ? *typeIdPtr : CreateType(&mask);
	Type* const type = &types[typeId];
	Span<u32> rows = AddRows(type, n);

	Array<EntityId> result(temp, n);
	u32 i = 0;
	for (; i < n && freeEntityIdx; i++) {
		Entity* entity = &entities[freeEntityIdx];
		freeEntityIdx = entities[freeEntityIdx].nextFreeIdx;
		entity->typeId = typeId;
		entity->gen++;
		entity->row = rows[i];
		result[i] = (u64)(entity - entities.data) | ((u64)entity->gen << 32);
	}
	for (Entity* entity = entities.Extend(n - i); i < n; i++, entity++) {
		entity->typeId = typeId;
		entity->gen++;
		entity->row = rows[i];
		result[i] = (u64)(entity - entities.data) | ((u64)entity->gen << 32);
	}

	return entityIds;
}

//--------------------------------------------------------------------------------------------------

void DestroyEntities(Span<EntityId> entityIds) {
	for (u32 i = 0; i < entityIds.len; i++) {
		const u32 idx = (u32)(entityIds[i] & 0xffffffff);
		const u32 gen = (u32)(entityIds[i] >> 32);
		Assert(idx < entities.len);
		Entity* const entity = &entities[idx];
		Assert(entity->gen == gen);

		RemoveRow(&types[entity->typeId], entity->row);
		entity->gen++;
	}
}

//--------------------------------------------------------------------------------------------------

ComponentId CreateComponent(s8 name, u32 size) {
	Assert(componentsLen < MaxComponents);
	size = AlignUp(size, 8);
	components[componentsLen++] = {
		.name = Copy(perm, name),
		.size = size,
	};
}

//--------------------------------------------------------------------------------------------------

static bool Contains(const ComponentMask* m1, const ComponentMask* m2) {
	for (u32 i = 0; i < MaxComponents / 64; i++) {
		const u64 b1 = m1->bits[i];
		const u64 b2 = m2->bits[i];
		if ((b1 & b2) != b1) {
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------------------------------------------

void AddComponent(EntityId entityId, ComponentId componentId) {
	const u32 idx = (u32)(entityId & 0xffffffff);
	const u32 gen = (u32)(entityId >> 32);

	Assert(idx < entities.len);
	Entity* entity = &entities[idx];
	Assert(gen == entity->gen);
	Assert(componentId < componentsLen);

	const TypeId oldTypeId = entity->typeId;
	Type* const  oldType   = &types[oldTypeId];
	const u32    oldRow    = entity->row;

	if (oldType->componentMask.bits[componentId / 64] & (componentId & 63)) {
		return;
	}

	ComponentMask newComponentMask = oldType->componentMask;
	newComponentMask.bits[componentId / 64] |= (u64)1 << (componentId & 63);
	const TypeId* const newTypeIdPtr = componentMaskToTypeId.Find(newComponentMask);
	const u32 newTypeId = newTypeIdPtr ? *newTypeIdPtr : CreateType(&newComponentMask);
	Type* const newType = &types[newTypeId];

	// TODO: AddRow() singular?
	const u32 newRow = AddRows(oldType, 1)[0];
	CopyRow(oldType, oldRow, newType, newRow);
	RemoveRow(oldType, oldRow);
}

//--------------------------------------------------------------------------------------------------

void RemoveComponent(EntityId entityId, ComponentId componentId) {
}

//--------------------------------------------------------------------------------------------------

void* GetComponentData(EntityId entityId, ComponentId componentId) {
}

//--------------------------------------------------------------------------------------------------

struct Iter {
	TypeMask     typeMask;
	u32          typeMaskIdx;
	u64          typeMaskBits;
	ComponentId* componentIds;
	void**       components;
	u32          componentsLen;
};

Iter* Query(Span<ComponentId> componentIds) {
	const ComponentMask componentMask = BuildComponentMask(componentIds);
	const TypeMask* typeMaskPtr = componentMaskToTypeMask.Find(componentMask);
	if (!typeMaskPtr) {
		TypeMask typeMask = {};
		for (u32 i = 0; i < typesLen; i++) {
			if (Contains(&types[i].componentMask, &componentMask)) {
				typeMask.bits[i / 64] |= (u64)1 << (i & 63);
			}
		}
		typeMaskPtr = componentMaskToTypeMask.Put(componentMask, typeMask);
	}

	Iter* iter = temp->AllocT<Iter>();
	iter->typeMask      = ;
	iter->typeMaskIdx   = ;
	iter->typeMaskBits  = ;
	iter->componentIds  = ;
	iter->components    = ;
	iter->componentsLen = ;

	return iter;
}
//--------------------------------------------------------------------------------------------------

Span<void*> Next(Iter* iter) {
	for (; iter->typeMaskIdx < MaxTypes / 64; iter->typeMaskIdx++) {
		if (!iter->typeMaskBits) { continue; }

		if (const u32 bit = Bsf64(iter->typeMaskBits)) {
			iter->typeMaskBits &= ~((u64)1 << bit);

			const TypeId typeId = (iter->typeMaskIdx * 64) + bit;
			Type* const type = &types[typeId];

			for (u32 i = 0; i < iter->componentsLen; i++) {
				u32 colIdx = *typeIdComponentIdToColumn.Find(typeId | (iter->componentIds[i] << TypeIdShift));
				iter->components[i] = type->data + (type->cap * type->columns[colIdx].offset);
			}

			return Span<void*>(iter->components, iter->componentsLen);
		}
	}
	return Span<void*>();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity