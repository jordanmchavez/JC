#include "JC/Entity.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Fmt.h"
#include "JC/Hash.h"
#include "JC/Map.h"
#include "JC/UnitTest.h"

namespace JC::Entity {

struct A { char b; u8 data[  48]; char e; };
struct B { char b; u8 data[  91]; char e; };
struct C { char b;                char e; };
struct D { char b; u8 data[5000]; char e; };

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
	Str           name;
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
	u64*         bits;
	u64*         bitsEnd;
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

	const u32 maxIdx = (componentObjsLen + 63) >> 6;
	Array<char> name(tempAllocator);
	for (u32 i = 0; i < maxIdx; i++) {
		u64 bits = mask->bits[i];
		while (bits) {
			const u32 bit = Bit::Bsf64(bits);
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

			Fmt::Printf(&name, "{},", componentObj->name);
		}
	}
	if (name.len > 0) { name.len--; } else { Fmt::Printf(&name, "<empty>"); }
	type->data = (u8*)allocator->Alloc(TypeInitialCap * type->rowSize);
	type->len  = 0;
	type->cap  = TypeInitialCap;
	type->name = Copy(allocator, Str(name.data, name.len));

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

	((Entity*)type->data)[row] = ((Entity*)type->data)[lastRow];
	for (u32 c = 0; c < type->columnsLen; c++) {
		Column* col = &type->columns[c];
		memcpy(
			type->data + (col->offset * type->cap) + (row     * col->size),
			type->data + (col->offset * type->cap) + (lastRow * col->size),
			col->size
		);
	}
	type->len--;
}

//--------------------------------------------------------------------------------------------------

static void CopyRow(Type* srcType, u32 srcRow, Type* dstType, u32 dstRow) {
	((Entity*)dstType->data)[dstRow] = ((Entity*)srcType->data)[srcRow];
	u32 srcColIdx = 0;
	u32 dstColIdx = 0;
	while (srcColIdx < srcType->columnsLen && dstColIdx < dstType->columnsLen) {
		const Column* const srcCol = &srcType->columns[srcColIdx];
		const Column* const dstCol = &dstType->columns[dstColIdx];
		if (srcCol->componentId == dstCol->componentId) {
			memcpy(
			    dstType->data + (dstCol->offset * dstType->cap) + (dstRow * dstCol->size),
				srcType->data + (srcCol->offset * srcType->cap) + (srcRow * srcCol->size),
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
	const TypeId* const typeIdPtr = componentMaskHashToTypeId.FindOrNull(maskHash);
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
		const u64 entityId = (u64)(entityObj - entityObjs.data) | ((u64)entityObj->gen << 32);;
		((Entity*)type->data)[rows[i]].id = entityId;
		result[i].id = entityId;
	}
	for (EntityObj* entityObj = entityObjs.Extend(n - i); i < n; i++, entityObj++) {
		entityObj->typeId = typeId;
		entityObj->gen++;
		entityObj->row = rows[i];
		const u64 entityId = (u64)(entityObj - entityObjs.data) | ((u64)entityObj->gen << 32);;
		((Entity*)type->data)[rows[i]].id = entityId;
		result[i].id = entityId;
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

static EntityObj* GetEntityObj(Entity entity) {
	const u32 idx = (u32)(entity.id & 0xffffffff);
	const u32 gen = (u32)(entity.id >> 32);
	Assert(idx < entityObjs.len);
	EntityObj* const entityObj = &entityObjs[idx];
	Assert(gen == entityObj->gen);
	return entityObj;
}

//--------------------------------------------------------------------------------------------------

void AddComponents(Entity entity, Span<Component> components) {
	EntityObj* const entityObj = GetEntityObj(entity);
	Type* const oldType = &types[entityObj->typeId];

	ComponentMask newMask = oldType->componentMask;
	for (u64 i = 0; i < components.len; i++) {
		newMask.bits[components[i].id / 64] |= (u64)1 << (components[i].id & 63);
	}
	const TypeId* const newTypeIdPtr = componentMaskHashToTypeId.FindOrNull(Hash(&newMask, sizeof(newMask)));
	const TypeId newTypeId = newTypeIdPtr ? *newTypeIdPtr : CreateType(&newMask);
	Type* const newType = &types[newTypeId];

	if (oldType != newType) {
		const u32 oldRow = entityObj->row;
		const u32 newRow = AddRows(newType, 1)[0];
		CopyRow(oldType, oldRow, newType, newRow);
		RemoveRow(oldType, oldRow);
		entityObj->typeId = newTypeId;
		entityObj->row    = newRow;
	}
}

//--------------------------------------------------------------------------------------------------

void RemoveComponents(Entity entity, Span<Component> components) {
	EntityObj* const entityObj = GetEntityObj(entity);
	Type* const oldType = &types[entityObj->typeId];

	ComponentMask newMask = oldType->componentMask;
	for (u64 i = 0; i < components.len; i++) {
		newMask.bits[components[i].id / 64] &= ~((u64)1 << (components[i].id & 63));
	}
	const TypeId* const newTypeIdPtr = componentMaskHashToTypeId.FindOrNull(Hash(&newMask, sizeof(newMask)));
	const TypeId newTypeId = newTypeIdPtr ? *newTypeIdPtr : CreateType(&newMask);
	Type* const newType = &types[newTypeId];

	if (oldType != newType) {
		const u32 oldRow = entityObj->row;
		const u32 newRow = AddRows(newType, 1)[0];
		CopyRow(oldType, oldRow, newType, newRow);
		RemoveRow(oldType, oldRow);
		entityObj->typeId = newTypeId;
		entityObj->row    = newRow;
	}
}

//--------------------------------------------------------------------------------------------------

Span<void*> GetComponentData(Entity entity, Span<Component> components) {
	EntityObj* const entityObj = GetEntityObj(entity);
	Type* const type = &types[entityObj->typeId];

	void** data = tempAllocator->AllocT<void*>(components.len);
	for (u64 i = 0; i < components.len; i++) {
		const u32 key = entityObj->typeId | (components[i].id << MaxTypesShift);
		const Column column = type->columns[*typeIdComponentIdToColumnId.FindChecked(key)];
		data[i] = type->data + (type->cap * column.offset) + (entityObj->row * column.size);
	}

	return Span<void*>(data, components.len);
}

//--------------------------------------------------------------------------------------------------

Query CreateQuery(Span<Component> components) {
	Assert(components.len < MaxQueryComponents);

	const ComponentMask mask = BuildComponentMask(components);
	const PreHash maskHash = Hash(&mask, sizeof(mask));

	QueryId* queryIdPtr = componentMaskHashToQueryId.FindOrNull(maskHash);
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
	iter->bits          = iter->typeMask.bits;
	iter->bitsEnd       = iter->typeMask.bits + ((typesLen + 63) / 64);

	return iter;
}

//--------------------------------------------------------------------------------------------------

RowSet* Next(Iter* iter) {
	for (; iter->bits < iter->bitsEnd; iter->bits++) {
		while (*iter->bits) {
			const u32 bit = Bit::Bsf64(*iter->bits);
			*iter->bits &= ~((u64)1 << bit);

			const TypeId typeId = ((u32)(iter->bits - iter->typeMask.bits) * 64) + bit;
			Type* const type = &types[typeId];
			if (type->len == 0) { continue; }

			iter->rowSet.entities = (Entity*)type->data;
			for (u32 i = 0; i < iter->componentsLen; i++) {
				ColumnId colId = *typeIdComponentIdToColumnId.FindChecked(typeId | (iter->components[i].id << MaxTypesShift));
				iter->rowSet.componentData[i] = type->data + (type->cap * type->columns[colId].offset);
			}
			iter->rowSet.len = type->len;

			return &iter->rowSet;
		}
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------

UnitTest("Entity") {
A aa;aa;
B bb;bb;

	Init(testAllocator, testAllocator);

	const Component a = CreateComponent("A", sizeof(A));
	const Component b = CreateComponent("B", sizeof(B));
	const Component c = CreateComponent("C", sizeof(C));
	const Component d = CreateComponent("D", sizeof(D));

	Entity e = CreateEntities(1, {})[0];
	AddComponents(e, { a, b });
	Span<void*> data = GetComponentData(e, { a, b });
	*(A*)data[0] =  { .b = 'a', .e = 'a' };
	*(B*)data[1] =  { .b = 'b', .e = 'b' };

	AddComponents(e, { c, d });
	data = GetComponentData(e, { a, b, c, d });
	CheckEq(((A*)data[0])->b, 'a');
	CheckEq(((A*)data[0])->e, 'a');
	CheckEq(((B*)data[1])->b, 'b');
	CheckEq(((B*)data[1])->e, 'b');
	*(C*)data[2] =  { .b = 'c', .e = 'c' };
	*(D*)data[3] =  { .b = 'd', .e = 'd' };

	RemoveComponents(e, { c });
	data = GetComponentData(e, { a, b, d });
	CheckEq(((A*)data[0])->b, 'a');
	CheckEq(((A*)data[0])->e, 'a');
	CheckEq(((B*)data[1])->b, 'b');
	CheckEq(((B*)data[1])->e, 'b');
	CheckEq(((D*)data[2])->b, 'd');
	CheckEq(((D*)data[2])->e, 'd');

	Span<Entity> e_a   = CreateEntities(1, { a       });
	Span<Entity> e_b   = CreateEntities(2, {    b    });
	Span<Entity> e_c   = CreateEntities(3, {       c });
	Span<Entity> e_ab  = CreateEntities(2, { a, b    });
	Span<Entity> e_ac  = CreateEntities(4, { a,    c });
	Span<Entity> e_bc  = CreateEntities(1, {    b, c });
	Span<Entity> e_abc = CreateEntities(3, { a, b, c });

	Query q_ab = CreateQuery({ a, b });
	Iter* i = RunQuery(q_ab);
	RowSet* rs = Next(i);
	if (CheckTrue(rs)) {
		CheckEq(rs->len, 2u);
		CheckEq(rs->entities[0].id, e_ab[0].id);
		CheckEq(rs->entities[1].id, e_ab[1].id);
	}
	rs = Next(i);
	if (CheckTrue(rs)) {
		CheckEq(rs->len, 1u);
		CheckEq(rs->entities[0].id, e.id);
	}
	rs = Next(i);
	if (CheckTrue(rs)) {
		CheckEq(rs->len, 3u);
		CheckEq(rs->entities[0].id, e_abc[0].id);
		CheckEq(rs->entities[1].id, e_abc[1].id);
		CheckEq(rs->entities[2].id, e_abc[2].id);
	}
	CheckFalse(Next(i));

	RemoveComponents(e_ab[1],  { a });
	RemoveComponents(e_abc[0], { b });

	i = RunQuery(q_ab);
	rs = Next(i);
	if (CheckTrue(rs)) {
		CheckEq(rs->len, 1u);
		CheckEq(rs->entities[0].id, e_ab[0].id);
	}
	rs = Next(i);
	if (CheckTrue(rs)) {
		CheckEq(rs->len, 1u);
		CheckEq(rs->entities[0].id, e.id);
	}
	rs = Next(i);
	if (CheckTrue(rs)) {
		CheckEq(rs->len, 2u);
		CheckEq(rs->entities[0].id, e_abc[2].id);	// 2,1 not 1,2 because unordered remove
		CheckEq(rs->entities[1].id, e_abc[1].id);
	}
	CheckFalse(Next(i));

	Query q_cd = CreateQuery({ c, d });
	i = RunQuery(q_cd);
	CheckFalse(Next(i));
}


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Entity