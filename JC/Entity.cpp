#include "JC/Entity.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Hash.h"
#include "JC/Map.h"
#include "JC/UnitTest.h"

namespace JC::Entity {

//--------------------------------------------------------------------------------------------------

// Win64 virtual reserve overhead:
// 64k = 1 bit
// 512k = 1 byte
// 1 gb = 2k
// 16gb = 32k

static constexpr u32 MaxComponents       = 1024;
static constexpr u32 MaxTypesShift       = 10;
static constexpr u32 MaxTypes            = 1 << MaxTypesShift;
static constexpr u32 MaxTypeColumns      = 64;
static constexpr u32 MaxQueries          = 1024;
static constexpr u64 EntitiesReserveSize = 1024 * 1024 * 1024;
static constexpr u64 TypeDataReserveSize = 1024 * 1024 * 1024;
static constexpr u32 TypeInitialCap      = 16;

using EntityId    = u64;
using ComponentId = u32;
using QueryId     = u32;
using TypeId      = u32;
using ColumnId    = u32;

struct ComponentMask {
	u64 bits[MaxComponents / 64];
};

struct TypeMask {
	u64 bits[MaxTypes / 64];
};

struct ComponentObj {
	Str name;
	u32 size;
};

struct Column {
	ComponentId componentId;
	u32         offset;
	u32         size;
};

struct Type {
	ComponentMask componentMask;
	u32           rowSize;
	Column        columns[MaxTypeColumns];	// TODO: we could very easily make this dynamic to remove the limit
	u32           columnsLen;
	u8*           data;
	u32           len;
	u32           cap;
};

struct EntityObj {
	union {
		u32 typeId;
		u32 nextFreeIdx;
	};
	u32 gen;
	u32 row;
};

struct QueryObj {
	Component     components[MaxQueryComponents];
	u32           componentsLen;
	ComponentMask componentMask;
	TypeMask      typeMask;
};

struct Iter {
	Component    components[MaxQueryComponents];
	u32          componentsLen;
	TypeMask     typeMask;
	u32          idx;
	RowSet       rowSet;
};

static Mem::Allocator*              allocator;
static Mem::TempAllocator*          tempAllocator;
static Array<EntityObj>             entityObjs;
static u32                          freeEntityObjIdx;
static ComponentObj                 componentObjs[MaxComponents];
static u32                          componentObjsLen;
static Type                         types[MaxTypes];
static u32                          typesLen;
static Map<u32, ColumnId>           typeIdComponentIdToColumnId;	// key = typeId | (componentId << TypeIdShift)
static Map<PreHash, TypeId>         componentMaskHashToTypeId;
static QueryObj                     queryObjs[MaxQueries];
static u32                          queryObjsLen;
static Map<PreHash, QueryId>        componentMaskHashToQueryId;

//--------------------------------------------------------------------------------------------------

static bool Contains(const ComponentMask* m1, const ComponentMask* m2) {
	const u32 bitsLen = (componentObjsLen + 63) >> 6;
	for (u32 i = 0; i < bitsLen; i++) {
		const u64 b1 = m1->bits[i];
		const u64 b2 = m2->bits[i];
		if ((b1 | b2) != b1) {
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------------------------------------------

static TypeId CreateType(const ComponentMask* mask) {
	Assert(typesLen < MaxTypes);
	const TypeId typeId = typesLen;
	Type* const type = &types[typesLen++];

	type->componentMask = *mask;
	type->rowSize       = sizeof(EntityId);

	const u32 bitsLen = (componentObjsLen + 63) >> 6;
	for (u32 i = 0; i < bitsLen; i++) {
		u64 bits = mask->bits[i];
		if (!bits) { continue; }

		while (const u32 bit = Bit::Bsf64(bits)) {
			bits &= ~((u64)1 << bit);

			u32 componentId = (i * 64) + bit;
			typeIdComponentIdToColumnId.Put(
				typeId | (componentId << MaxTypesShift),
				type->columnsLen
			);

			ComponentObj* componentObj = &componentObjs[componentId];
			type->columns[type->columnsLen++] = {
				.componentId = componentId,
				.offset      = type->rowSize,
				.size        = componentObj->size,
			};

			type->rowSize += componentObj->size;
		}
	}
	type->data  = (u8*)allocator->Alloc(TypeInitialCap * type->rowSize);
	type->len   = 0;
	type->cap   = TypeInitialCap;

	const PreHash maskHash = Hash(mask, sizeof(*mask));
	componentMaskHashToTypeId.Put(maskHash, typeId);

	for (u32 i = 0; i < queryObjsLen; i++) {
		if (Contains(mask, &queryObjs[i].componentMask)) {
			queryObjs[i].typeMask.bits[typeId / 64] |= (u64)1 << (typeId & 63);
		}
	}

	return typeId;
}

//--------------------------------------------------------------------------------------------------

void Init(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn) {
	allocator     = allocatorIn;
	tempAllocator = tempAllocatorIn;

	typeIdComponentIdToColumnId.Init(allocator);
	componentMaskHashToTypeId.Init(allocator);
	componentMaskHashToQueryId.Init(allocator);

	entityObjs.Init(allocator);
	entityObjs.Add();	// reserve 0 for invalid

	const ComponentMask emptyMask = {};
	const TypeId emptyTypeId = CreateType(&emptyMask);
	Assert(emptyTypeId == 0);

	componentObjsLen = 1;	// reserve 0 for invalid
	queryObjsLen = 1;	// reserve 0 for invalid
}

//--------------------------------------------------------------------------------------------------

static ComponentMask BuildComponentMask(Span<Component> components) {
	ComponentMask mask = {};
	for (u64 i = 0; i < components.len; i++) {
		const ComponentId componentId = components[i].id;
		mask.bits[componentId / 64] |= (u64)1 << (componentId & 63);
	}
	return mask;
}

//--------------------------------------------------------------------------------------------------

static Span<u32> AddRows(Type* type, u32 n) {
	const u32 oldLen = type->len;
	const u32 newLen = type->len + n;
	if (newLen > type->cap) {
		const u32 newCap = Max(newLen, type->cap * 2);
		u8* newData = (u8*)allocator->Alloc(newCap * type->rowSize);
		// Don't need to move the entities row
		for (u32 i = 0; i < type->columnsLen; i++) {
			const Column* const col = &type->columns[i];
			memmove(
				newData    + (type->cap * col->offset),
				type->data + (newCap    * col->offset),
				type->len * col->size
			);
		}
		allocator->Free(type->data, type->cap * type->rowSize);
		type->data = newData;
		type->cap = newCap;
	}
	type->len = newLen;

	Array<u32> rows(tempAllocator);
	rows.Resize(n);
	u32 row = oldLen;
	for (u32 i = 0; i < n; i++) {
		rows[i] = row++;
	}

	return rows;
}

//--------------------------------------------------------------------------------------------------

static void RemoveRow(Type* type, u32 row) {
	const u32 lastRow = type->len - 1;

	((EntityId*)type->data)[row] = ((EntityId*)type->data)[lastRow];
	for (u32 c = 0; c < type->columnsLen; c++) {
		Column* col = &type->columns[c];
		memcpy(
			type->data + (col->offset * type->cap) + (lastRow * col->size),
			type->data + (col->offset * type->cap) + (row     * col->size),
			col->size
		);
	}
	type->len--;
}

//--------------------------------------------------------------------------------------------------

static void CopyRow(Type* srcType, u32 srcRow, Type* dstType, u32 dstRow) {
	((EntityId*)srcType->data)[srcRow] = ((EntityId*)dstType->data)[dstRow];
	u32 srcColIdx = 0;
	u32 dstColIdx = 0;
	while (srcColIdx < srcType->columnsLen && dstColIdx < dstType->columnsLen) {
		const Column* const srcCol = &srcType->columns[srcColIdx];
		const Column* const dstCol = &dstType->columns[dstColIdx];
		if (srcCol->componentId == dstCol->componentId) {
			memcpy(
				srcType->data + (srcCol->offset * srcType->cap) + (srcRow * srcCol->size),
			    dstType->data + (dstCol->offset * dstType->cap) + (dstRow * dstCol->size),
				componentObjs[srcCol->componentId].size
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

Span<Entity> CreateEntities(u32 n, Span<Component> components) {
	const ComponentMask mask = BuildComponentMask(components);
	const PreHash maskHash = Hash(&mask, sizeof(mask));
	const TypeId* const typeIdPtr = componentMaskHashToTypeId.Find(maskHash);
	TypeId typeId = typeIdPtr ? *typeIdPtr : CreateType(&mask);
	Type* const type = &types[typeId];
	Span<u32> rows = AddRows(type, n);

	Array<Entity> result(tempAllocator);
	result.Resize(n);
	u32 i = 0;
	for (; i < n && freeEntityObjIdx; i++) {
		EntityObj* entityObj = &entityObjs[freeEntityObjIdx];
		freeEntityObjIdx = entityObjs[freeEntityObjIdx].nextFreeIdx;
		entityObj->typeId = typeId;
		entityObj->gen++;
		entityObj->row = rows[i];
		result[i].id = (u64)(entityObj - entityObjs.data) | ((u64)entityObj->gen << 32);
	}
	for (EntityObj* entityObj = entityObjs.Extend(n - i); i < n; i++, entityObj++) {
		entityObj->typeId = typeId;
		entityObj->gen++;
		entityObj->row = rows[i];
		result[i].id = (u64)(entityObj - entityObjs.data) | ((u64)entityObj->gen << 32);
	}

	return result;
}

//--------------------------------------------------------------------------------------------------

void DestroyEntities(Span<Entity> entities) {
	for (u32 i = 0; i < entities.len; i++) {
		const u32 idx = (u32)(entities[i].id & 0xffffffff);
		const u32 gen = (u32)(entities[i].id >> 32);
		Assert(idx < entityObjs.len);
		EntityObj* const entityObj = &entityObjs[idx];
		Assert(entityObj->gen == gen);

		RemoveRow(&types[entityObj->typeId], entityObj->row);
		entityObj->gen++;
		entityObj->nextFreeIdx = freeEntityObjIdx;
		freeEntityObjIdx = idx;
	}
}

//--------------------------------------------------------------------------------------------------

Component CreateComponent(Str name, u32 size) {
	Assert(componentObjsLen < MaxComponents);
	size = (u32)Bit::AlignUp(size, 8);
	componentObjs[componentObjsLen++] = {
		.name = Copy(allocator, name),
		.size = size,
	};
	return Component { .id = componentObjsLen - 1 };
}

//--------------------------------------------------------------------------------------------------

void AddComponent(Entity entity, Component component) {
	const u32 idx = (u32)(entity.id & 0xffffffff);
	const u32 gen = (u32)(entity.id >> 32);

	Assert(idx < entityObjs.len);
	EntityObj* const entityObj = &entityObjs[idx];
	Assert(gen == entityObj->gen);
	Assert(component.id < componentObjsLen);

	const TypeId oldTypeId = entityObj->typeId;
	Type* const  oldType   = &types[oldTypeId];
	const u32    oldRow    = entityObj->row;

	if (oldType->componentMask.bits[component.id / 64] & (component.id & 63)) {
		return;
	}

	ComponentMask newMask = oldType->componentMask;
	newMask.bits[component.id / 64] |= (u64)1 << (component.id & 63);
	const TypeId* const newTypeIdPtr = componentMaskHashToTypeId.Find(Hash(&newMask, sizeof(newMask)));
	const u32 newTypeId = newTypeIdPtr ? *newTypeIdPtr : CreateType(&newMask);
	Type* const newType = &types[newTypeId];

	// TODO: AddRow() singular?
	const u32 newRow = AddRows(oldType, 1)[0];
	CopyRow(oldType, oldRow, newType, newRow);
	RemoveRow(oldType, oldRow);
}

//--------------------------------------------------------------------------------------------------

void RemoveComponent(Entity entity, Component component) {
	entity;component;
}

//--------------------------------------------------------------------------------------------------

void* ComponentData(Entity entity, Component component) {
	entity;component;
	return 0;
}

//--------------------------------------------------------------------------------------------------

Query CreateQuery(Span<Component> components) {
	Assert(components.len < MaxQueryComponents);

	const ComponentMask mask = BuildComponentMask(components);
	const PreHash maskHash = Hash(&mask, sizeof(mask));

	QueryId* queryIdPtr = componentMaskHashToQueryId.Find(maskHash);
	if (queryIdPtr) {
		return Query { .id = *queryIdPtr };
	}

	Assert(queryObjsLen < MaxQueries);
	QueryObj* const queryObj = &queryObjs[queryObjsLen];
	const QueryId queryId = queryObjsLen++;

	queryObj->componentMask = mask;
	memcpy(queryObj->components, components.data, components.len * sizeof(components[0]));
	queryObj->componentsLen = (u32)components.len;
	for (u32 i = 0; i < typesLen; i++) {
		if (Contains(&types[i].componentMask, &mask)) {
			queryObj->typeMask.bits[i / 64] |= (u64)1 << (i & 63);
		}
	}

	componentMaskHashToQueryId.Put(maskHash, queryId);

	return Query { .id = queryId };
}
//--------------------------------------------------------------------------------------------------

Iter* RunQuery(Query query) {
	Assert(query.id && query.id < queryObjsLen);
	QueryObj* const queryObj = &queryObjs[query.id];

	Iter* iter = tempAllocator->AllocT<Iter>();
	memcpy(iter->components, queryObj->components, queryObj->componentsLen * sizeof(ComponentId));
	iter->componentsLen = queryObj->componentsLen;
	iter->typeMask      = queryObj->typeMask; 
	iter->idx           = 0;

	return iter;
}

//--------------------------------------------------------------------------------------------------

RowSet* Next(Iter* iter) {
	for (; iter->idx < MaxTypes / 64; iter->idx++) {
		u64* const bits = iter->typeMask.bits;
		if (!bits[iter->idx]) { continue; }

		if (const u32 bit = Bit::Bsf64(bits[iter->idx])) {
			bits[iter->idx] &= ~((u64)1 << bit);

			const TypeId typeId = (iter->idx * 64) + bit;
			Type* const type = &types[typeId];

			iter->rowSet.entities = (Entity*)&type->data;
			for (u32 i = 0; i < iter->componentsLen; i++) {
				ColumnId colId = *typeIdComponentIdToColumnId.Find(typeId | (iter->components[i].id << MaxTypesShift));
				iter->rowSet.componentData[i] = type->data + (type->cap * type->columns[colId].offset);
			}
			iter->rowSet.len = type->len;

			return &iter->rowSet;
		}
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------

struct A { u8 data[64]; };
struct B { u8 data[91]; };
struct C { u8 data[1]; };

UnitTest("Entity") {
	Init(testAllocator, testAllocator);

	const Component a = CreateComponent("A", sizeof(A));
	const Component b = CreateComponent("B", sizeof(B));
	const Component c = CreateComponent("C", sizeof(C));

	Entity e = CreateEntities(1, {})[0];
	AddComponent(e, a);
	AddComponent(e, b);
	AddComponent(e, c);

	Span<Entity> e_a   = CreateEntities(1, { a       });
	Span<Entity> e_b   = CreateEntities(2, {    b    });
	Span<Entity> e_c   = CreateEntities(3, {       c });
	Span<Entity> e_ab  = CreateEntities(2, { a, b    });
	Span<Entity> e_ac  = CreateEntities(4, { a,    c });
	Span<Entity> e_bc  = CreateEntities(1, {    b, c });
	Span<Entity> e_abc = CreateEntities(3, { a, b, c });

	Query q = CreateQuery({ a, b });
	Iter* i = RunQuery(q);
	RowSet* rs = Next(i); CheckTrue(rs);
	CheckEq(rs->len, 2u);
	CheckEq(rs->entities[0].id, e_ab[0].id);
	CheckEq(rs->entities[1].id, e_ab[1].id);
	rs = Next(i); CheckTrue(rs);
	CheckEq(rs->len, 3u);
	CheckEq(rs->entities[0].id, e_abc[0].id);
	CheckEq(rs->entities[1].id, e_abc[1].id);
	CheckEq(rs->entities[2].id, e_abc[2].id);
	rs = Next(i); CheckFalse(rs);

	//Span<Entity> CreateEntities(u32 n, Span<Component> components);
	//void         DestroyEntities(Span<Entity> entitys);

	//Component    CreateComponent(s8 name, u32 len);
	//void         AddComponent(Entity entity, Component component);
	//void         RemoveComponent(Entity entity, Component component);
	//void*        ComponentData(Entity entity, Component component);

	//Query        CreateQuery(Span<Component> components);
	//Iter*        RunQuery(Query query);
	//RowSet*      Next(Iter* iter);
}


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity