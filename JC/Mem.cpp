#include "JC/Mem.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Map.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct MemObj : Mem {
	s8      name     = {};
	u64     bytes    = 0;
	u32     allocs   = 0;
	u32     children = 0;
	MemObj* parent   = 0;

	void* Alloc(u64 size, SrcLoc sl) override;
	void* Realloc(void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) override;
	void  Free(void* ptr, u64 size) override;
};

//--------------------------------------------------------------------------------------------------

struct TempMemObj : TempMem {
	void* Alloc(u64 size, SrcLoc sl) override;
	void* Realloc(void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) override;
	u64   Mark() override;
	void  Reset(u64 mark) override;
};

//--------------------------------------------------------------------------------------------------

struct Block {
    Block* prevPhys = 0;
    u64    size     = 0;
    Block* nextFree = 0;
    Block* prevFree = 0;
};

struct Index {
	u32 f;
	u32 s;
};

static constexpr u32 AlignSizeLog2       = 3;
static constexpr u32 AlignSize           = 1 << AlignSizeLog2;
static constexpr u32 SecondCountLog2     = 4;
static constexpr u32 SecondCount         = 1 << SecondCountLog2;    // 16
static constexpr u32 FirstShift          = SecondCountLog2 + AlignSizeLog2; // 8
static constexpr u32 FirstMax            = 64;
static constexpr u32 FirstCount          = FirstMax - FirstShift + 1;  // 64 - 8 + 1 = 57
static constexpr u32 SmallBlockSize      = 1 << FirstShift; // 256
static constexpr u32 BlockHeaderOverhead = sizeof(u64);	// 8
static constexpr u64 BlockOverhead       = sizeof(Block*);
static constexpr u64 BlockSizeMin        = sizeof(Block) - BlockOverhead;	// 24
static constexpr u64 BlockSizeMax        = ((u64)1 << FirstMax) - BlockOverhead;	// 128 extabytes
static constexpr u64 BlockFreeBit        = 1 << 0;
static constexpr u64 BlockPrevFreeBit    = 1 << 1;
static constexpr u64 BlockSizeMask       = ~(BlockFreeBit | BlockPrevFreeBit);
static constexpr u64 BlockStartOffset    = 16;	// right after Block.size

static_assert(AlignSize == SmallBlockSize / SecondCount);

//--------------------------------------------------------------------------------------------------

struct Trace {
	MemObj* mem    = 0;
	SrcLoc  sl     = {};
	u32     allocs = 0;
	u64     bytes  = 0;
};

//--------------------------------------------------------------------------------------------------

static constexpr u64   AlignUp   (u64   u, u64 align) { return (u + (align - 1)) & ~(align - 1); }
static constexpr u64   AlignDown (u64   u, u64 align) { return u & ~(align - 1); }
static constexpr void* AlignPtrUp(void* p, u64 align) {	return (void*)AlignUp((u64)p, align); }

//--------------------------------------------------------------------------------------------------

struct MemApiObj : MemApi {
	static constexpr u32 MaxMemObjs                  = 1024;

	u8*              permBegin                       = 0;
	u8*              permCommit                      = 0;
	u8*              permEnd                         = 0;
    Block            nullBlock                       = {};
    u64              first                           = 0;
    u64              second[FirstCount]              = {};
    Block*           blocks[FirstCount][SecondCount] = {};

	MemObj           memObjs[MaxMemObjs]             = {};
	MemObj*          freeMemObjs                     = 0;	// parent is the "next" link field

	Array<Trace>     traces                          = {};
	Map<u64, u64>    srcLocToTrace                   = {};
	Map<void*, u64>  ptrToTrace                      = {};
	MemLeakReporter* reporter                        = 0;

	u8*              tempBegin                       = 0;
	u8*              tempUsed                        = 0;
	u8*              tempCommit                      = 0;
	u8*              tempEnd                         = 0;

	//----------------------------------------------------------------------------------------------

	void AddTrace(MemObj* mem, void* ptr, u64 size, SrcLoc sl) {
		u64 key = HashCombine(Hash(sl.file), &sl.line, sizeof(sl.line));
		if (Opt<u64> idx = srcLocToTrace.Find(key)) {
			Trace* trace = &traces[idx.val];
			trace->allocs++;
			trace->bytes += size;
			ptrToTrace.Put(ptr, idx.val);
		} else {
			traces.Add(Trace {
				.mem    = mem,
				.sl     = sl,
				.allocs = 1,
				.bytes  = size,
			});
			ptrToTrace.Put(ptr, traces.len - 1);
			srcLocToTrace.Put(key, traces.len - 1);
		}
	}

	//----------------------------------------------------------------------------------------------

	void RemoveTrace(void* ptr, u64 size) {
		if (Opt<u64> idx = ptrToTrace.Find(ptr); idx.hasVal) {
			Trace* trace = &traces[idx.val];
			Assert(trace->allocs > 0);
			trace->allocs--;
			trace->bytes -= size;
			ptrToTrace.Remove(ptr);
		}
	}

	//----------------------------------------------------------------------------------------------

	Index CalcIndex(u64 size) {
		if (size < SmallBlockSize) {
			return Index {
				.f = 0,
				.s = (u32)size / (SmallBlockSize / SecondCount),
			};
		} else {
			const u64 roundedSize = size + (1 << (Bsr64(size) - SecondCountLog2)) - 1;
			const u32 bit = Bsr64(roundedSize);
			return Index {
				.f = bit - FirstShift + 1,
				.s = (u32)(roundedSize >> (bit - SecondCountLog2)) & ~(SecondCount - 1),
			};
		}
	}

	//----------------------------------------------------------------------------------------------

	void InsertFreeBlock(Block* block, Index idx) {
		Assert((u64)block % AlignSize  == 0);
		block->nextFree                  = blocks[idx.f][idx.s];
		block->prevFree                  = &nullBlock;
		blocks[idx.f][idx.s]->prevFree = block;
		blocks[idx.f][idx.s]           = block;
		first         |= (u64)1 << idx.f;
		second[idx.f] |= (u64)1 << idx.s;
	}

	//----------------------------------------------------------------------------------------------

	void Init(u64 permReserveSize, u64 tempReserveSize, MemLeakReporter* reporterIn) override {
		permBegin  = (u8*)Sys::VirtualReserve(permReserveSize);
		permCommit = permBegin;
		permEnd    = permBegin + permReserveSize;

		nullBlock.nextFree = &nullBlock;
		nullBlock.prevFree = &nullBlock;
		for (u32 i = 0; i < FirstCount; i++) {
			for (u32 j = 0; j < SecondCount; j++) {
				blocks[i][j] = &nullBlock;
			}
		}

		const u64 poolSize = AlignDown(permReserveSize - 2 * BlockOverhead, AlignSize);
		Block* const block = (Block*)(permBegin - BlockOverhead);
		block->size = poolSize | BlockFreeBit;
		Index idx = CalcIndex(poolSize);
		InsertFreeBlock(block, idx);
		Block* const next = (Block*)((u8*)block + poolSize - sizeof(Block*));
		next->prevPhys = block;
		next->size = 0 | BlockPrevFreeBit;

		freeMemObjs = &memObjs[1];	// allocators[0] is reserved for internal allocations
		for (u32 i = 1; i < MaxMemObjs - 1; i++) {
			memObjs[i].parent = &memObjs[i + 1];
		}
		memObjs[MaxMemObjs - 1].parent = nullptr;

		traces.mem = &memObjs[0];
		srcLocToTrace.Init(&memObjs[0]);
		ptrToTrace.Init(&memObjs[0]);
		reporter = reporterIn;

		tempBegin  = (u8*)Sys::VirtualReserve(tempReserveSize);
		tempUsed   = tempBegin;
		tempCommit = tempBegin;
		tempEnd    = tempBegin + tempReserveSize;
	}

	//----------------------------------------------------------------------------------------------

	void RemoveFreeBlock(Block* block, Index idx) {
		Block* const n = block->nextFree;
		Block* const p = block->prevFree;
		n->prevFree = p;
		p->nextFree = n;
		if (blocks[idx.f][idx.s] == block) {
			blocks[idx.f][idx.s] = n;
			if (n == &nullBlock) {
				second[idx.f] &= ~((u64)1 << idx.s);
				if (!second[idx.f]) {
					first &= ((u64)1 << idx.f);
				}
			}
		}
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(MemObj* mem, u64 size, SrcLoc sl) {
		if (size == 0) {
			return 0;
		}
		size = AlignUp(size, AlignSize);

		Block* block = 0;
		Index idx = CalcIndex(size);
		Assert(idx.f <= FirstCount);

		u64 secondMap = second[idx.f] & ((u64)(-1) << idx.s);
		if (!secondMap) {
			const u32 firstMap = first & ((u64)(-1) << (idx.f + 1));
			if (!firstMap) {
				return nullptr;
			}
			idx.f = Bsf64(firstMap);
			secondMap = second[idx.f];
		}

		if (!secondMap) {
			Assert(permCommit < permEnd);
			Assert(size <= BlockSizeMax);
			const u64 commitSize = permCommit - permBegin;
			u64 commitExtend = Max((u64)4096, commitSize);
			while (commitExtend < size + BlockOverhead) {
				commitExtend *= 2;
				Assert(commitSize + commitExtend <= BlockSizeMax);
			}
			Sys::VirtualCommit(permCommit, commitExtend);

			Block* const lastBlock = (Block*)(permCommit - sizeof(Block*));

			permCommit += commitExtend;

		}

		idx.s = Bsf64(secondMap);
		block = blocks[idx.f][idx.s];
		Assert((block->size & BlockSizeMask) >= size);
		RemoveFreeBlock(block, idx);

		if ((block->size & BlockSizeMask) - size >= sizeof(Block)) {
			Block* const rem = (Block*)((u8*)block + size + 8);
			const u64 remSize = (block->size & BlockSizeMask) - size - 8;
			rem->size = remSize | BlockFreeBit;
			block->size = size;
			rem->prevPhys = block;
			idx = CalcIndex(remSize);
			InsertFreeBlock(rem, idx);
		} else {
			block->size &= ~(BlockFreeBit | BlockPrevFreeBit);
			((Block*)((u8*)block + block->size + 8))->size |= BlockPrevFreeBit;
		}

		void* const ptr = (u8*)block + 16;
		if (mem != &memObjs[0]) {
			AddTrace(mem, ptr, size, sl);
		}
		return ptr;
	}

	//----------------------------------------------------------------------------------------------

	void* Realloc(AllocatorImpl* a, void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) {
		if (ptr) {
			RemoveTrace(ptr, oldSize);
		}
		void* const newPtr = realloc((void*)ptr, newSize);
		Assert(newPtr != nullptr);
		if (a == &allocators[0]) {
			return newPtr;
		}
		AddTrace(a, newPtr, newSize, sl);
		return newPtr;
	}

	//----------------------------------------------------------------------------------------------

	void Free(void* ptr, u64 size) {
		if (!ptr) {
			return;
		}

		RemoveTrace(ptr, size);

		Block* block = (Block*)((u8*)ptr + 8);
		Assert(!(block->size & BlockFreeBit));
		block->size |= BlockFreeBit;

		Block* const next = (Block*)((u8*)block + (block->size & BlockSizeMask) + 8);
		next->size |= BlockPrevFreeBit;

		if (block->size & BlockPrevFreeBit) {
			Block* const prev = block->prevPhys;
			RemoveFreeBlock(prev, CalcIndex(prev->size & BlockSizeMask));
			prev->size += (block->size & BlockSizeMask) + 8;
			block = prev;
		}
		next->prevPhys = block;

		if (next->size & BlockFreeBit) {
			RemoveFreeBlock(next, CalcIndex(next->size & BlockSizeMask));
			block->size += (next->size & BlockSizeMask) + 8;
			((Block*)((u8*)block + block->size + 8))->prevPhys = block;
		}

		InsertFreeBlock(block, CalcIndex(block->size & BlockSizeMask));
	}

	//----------------------------------------------------------------------------------------------

	Allocator* Create(s8 name, Allocator* parent) override {
		JC_ASSERT(!parent || ((AllocatorImpl*)parent >= &allocators[1] && (AllocatorImpl*)parent <= &allocators[MaxMemObjs]));
		JC_ASSERT(freeAllocators);
		AllocatorImpl* const a = freeAllocators;
		freeAllocators = freeAllocators->parent;
		a->name     = name;
		a->bytes    = 0;
		a->allocs   = 0;
		a->children = 0;
		a->parent   = (AllocatorImpl*)parent;
		if (a->parent) {
			a->parent->children++;
		}
		return a;
	}

	//----------------------------------------------------------------------------------------------

	void Destroy(Allocator* allocator) override {
		AllocatorImpl* const a = (AllocatorImpl*)allocator;
		if (memLeakReporter) {
			memLeakReporter->Begin(a->name, a->bytes, a->allocs, a->children);
			if (a->bytes > 0 || a->allocs > 0) {
				for (u64 i = 0; i < traces.len; i++) {
					if (traces[i].allocator == a) {
						memLeakReporter->Alloc(traces[i].sl.file, traces[i].sl.line, traces[i].bytes, traces[i].allocs);
					}
				}
			}
			if (a->children > 0) {
				for (u32 i = 0; i < MaxMemObjs; i++) {
					if (allocators[i].parent == a) {
						memLeakReporter->Child(allocators[i].name, allocators[i].bytes, allocators[i].allocs);
					}
				}
			}
		}
		memset(a, 0, sizeof(AllocatorImpl));
		a->parent = freeAllocators;
		freeAllocators = a;
	}

	//----------------------------------------------------------------------------------------------

	void SetMemLeakReporter(MemLeakReporter* reporter) override {
		memLeakReporter = reporter;
	}
};

static AllocatorApiImpl allocatorApiImpl;

AllocatorApi* AllocatorApi::Get() {
	return &allocatorApiImpl;
}

//--------------------------------------------------------------------------------------------------

void* AllocatorImpl::Alloc(u64 size, SrcLoc sl) {
	return allocatorApiImpl.Alloc(this, size, sl);
}

void* AllocatorImpl::Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) {
	return allocatorApiImpl.Realloc(this, ptr, oldSize, newSize, sl);
}

void AllocatorImpl::Free(const void* ptr, u64 size) {
	allocatorApiImpl.Free(ptr, size);
}
//--------------------------------------------------------------------------------------------------

struct TempChunk {
	u8*        end  = nullptr;
	u8*        free = nullptr;
	u8*        last = nullptr;
	TempChunk* next = nullptr;
};

struct TempFreeList {
	TempChunk* chunks            = nullptr;
	u32        count             = 0;
	u32        minCountThisFrame = 0;
	u32        framesWithUnused  = 0;
};

//--------------------------------------------------------------------------------------------------

struct TempAllocatorImpl : TempAllocator {
	TempChunk* chunk = nullptr;
	void*      buf   = nullptr;

	void* Alloc(u64 size, SrcLoc sl) override;
	void* Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) override;
};

//--------------------------------------------------------------------------------------------------

static constexpr u64 TempMinChunkSizeLog2 = 16;	// 64k
static constexpr u64 TempMaxChunkSizeLog2 = 32;	// 4gb
static constexpr u64 TempMinChunkSize     = (u64)1 << TempMinChunkSizeLog2;
static constexpr u64 TempMaxChunkSize     = (u64)1 << TempMaxChunkSizeLog2;
static constexpr u64 TempMaxAllocSize     = TempMaxChunkSize - sizeof(TempChunk);
static constexpr u64 TempMaxChunkSizes    = TempMaxChunkSizeLog2 - TempMinChunkSizeLog2 + 1;	// inclusive
static constexpr u64 TempAllocAlign       = 8;

static_assert(sizeof(TempChunk) % TempAllocAlign == 0);
static_assert(alignof(TempChunk) == TempAllocAlign);

static_assert(sizeof(TempAllocatorImpl) % TempAllocAlign == 0);
static_assert(alignof(TempAllocatorImpl) == TempAllocAlign);

struct TempAllocatorApiImpl : TempAllocatorApi {
	VirtualMemoryApi* virtualMemoryApi = nullptr;
	TempFreeList      freeLists[TempMaxChunkSizes];
	u32               maxUnusedFrames = 30;	// not constexpr so we can test it
	u32               nextSizeToCheck = 0;

	//----------------------------------------------------------------------------------------------

	TempChunk* AllocChunk(u64 size) {
		JC_ASSERT(size >= TempMinChunkSize);
		const u32 i = Log2(size) - TempMinChunkSizeLog2;
		TempFreeList* const freeList = &freeLists[i];
		TempChunk* chunk = freeList->chunks;
		if (chunk) {
			freeList->chunks = chunk->next;
			freeList->count--;
			freeList->minCountThisFrame = Min(freeList->minCountThisFrame, freeList->count);
		} else {
			chunk = (TempChunk*)virtualMemoryApi->ReserveCommit(size);
		}
		chunk->end  = (u8*)chunk + size;
		chunk->free = (u8*)(chunk + 1);
		chunk->last = (u8*)(chunk + 1);
		chunk->next = nullptr;
		return chunk;
	}

	//----------------------------------------------------------------------------------------------

	void FreeChunk(TempChunk* chunk) {
		TempFreeList* const freeList = &freeLists[Log2(chunk->end - (u8*)chunk) - TempMinChunkSizeLog2];
		chunk->next = freeList->chunks;
		freeList->chunks = chunk;
		freeList->count++;
	}

	//----------------------------------------------------------------------------------------------

	void Init(VirtualMemoryApi* virtualMemoryApiIn) override {
		virtualMemoryApi = virtualMemoryApiIn;
		nextSizeToCheck = 0;
	}

	//----------------------------------------------------------------------------------------------
	
	TempAllocator* Create() override {
		TempChunk* const chunk = AllocChunk((u64)1 << TempMinChunkSizeLog2);

		TempAllocatorImpl ta;
		ta.chunk = chunk;
		ta.buf   = nullptr;
		memcpy(chunk->free, &ta, sizeof(ta));
		TempAllocatorImpl* const res = (TempAllocatorImpl*)chunk->free;

		chunk->free += sizeof(TempAllocatorImpl);
		chunk->last = chunk->free;

		return res;
	}

	//----------------------------------------------------------------------------------------------
	
	TempAllocator* Create(void* buf, u64 size) override {
		JC_ASSERT(size >= sizeof(TempChunk) + sizeof(TempAllocatorImpl));

		TempChunk* const chunk = (TempChunk*)buf;
		chunk->end  = (u8*)chunk + size;
		chunk->free = (u8*)chunk + sizeof(TempChunk);
		chunk->last = chunk->free;
		chunk->next = nullptr;

		TempAllocatorImpl ta;
		ta.chunk = chunk;
		ta.buf   = buf;
		memcpy(chunk->free, &ta, sizeof(ta));
		TempAllocatorImpl* const res = (TempAllocatorImpl*)chunk->free;

		chunk->free += sizeof(TempAllocatorImpl);
		chunk->last = chunk->free;

		return res;
	}

	//----------------------------------------------------------------------------------------------
	
	void Destroy(TempAllocator* ta) override {
		TempChunk* chunk = ((TempAllocatorImpl*)ta)->chunk;
		while (chunk->next) {
			TempChunk* next = chunk->next;
			FreeChunk(chunk);
			chunk = next;
		}
		if (chunk != ((TempAllocatorImpl*)ta)->buf) {
			FreeChunk(chunk);
		}
	}

	//----------------------------------------------------------------------------------------------

	void Reset(TempAllocator* ta) override {
		TempChunk* chunk = ((TempAllocatorImpl*)ta)->chunk;
		while (chunk->next) {
			TempChunk* next = chunk->next;
			FreeChunk(chunk);
			chunk = next;
		}
		((TempAllocatorImpl*)ta)->chunk = chunk;
		chunk->free = (u8*)chunk + sizeof(TempChunk) + sizeof(TempAllocatorImpl);
		chunk->last = chunk->free;
	}

	//----------------------------------------------------------------------------------------------

	void Frame() {
		for (u32 i = 0; i < TempMaxChunkSizes; i++) {
			freeLists[i].framesWithUnused = freeLists[i].minCountThisFrame ? freeLists[i].framesWithUnused + 1 : 0;
			freeLists[i].minCountThisFrame = freeLists[i].count;
		}
		TempFreeList* check = &freeLists[nextSizeToCheck];
		if (check->framesWithUnused > maxUnusedFrames) {
			TempChunk* chunk = check->chunks;
			JC_ASSERT(chunk);
			check->chunks = chunk->next;
			check->count--;
			check->minCountThisFrame = Min(check->minCountThisFrame, check->count);
			virtualMemoryApi->Free(chunk);
		}
		nextSizeToCheck++;
	}
};

TempAllocatorApiImpl tempAllocatorApiImpl;

TempAllocatorApi* TempAllocatorApi::Get() {
	return &tempAllocatorApiImpl;
}

//--------------------------------------------------------------------------------------------------

static constexpr u64 Align(u64 size, u64 align) {
	return (size + align - 1) & ~(align - 1);
}

//--------------------------------------------------------------------------------------------------

void* TempAllocatorImpl::Alloc(u64 size, SrcLoc) {
	size = Align(size, TempAllocAlign);
	JC_ASSERT(size <= TempMaxAllocSize);
	if (!chunk || chunk->free + size > chunk->end) {
		u64 newChunkSize = (chunk && chunk != buf) ? (chunk->end - (u8*)chunk) : TempMinChunkSize;
		do {
			newChunkSize *= 2;
		} while (newChunkSize < size + sizeof(TempChunk));
		JC_ASSERT(newChunkSize <= TempMaxChunkSize);
		TempChunk* const newChunk = tempAllocatorApiImpl.AllocChunk(newChunkSize);
		newChunk->next = chunk;
		chunk = newChunk;
	}
	chunk->last = chunk->free;
	chunk->free += size;
	return chunk->last;
}

//--------------------------------------------------------------------------------------------------

void* TempAllocatorImpl::Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc) {
	newSize = Align(newSize, TempAllocAlign);
	JC_ASSERT(newSize <= TempMaxAllocSize);
	if (chunk && chunk->last == ptr) {
		chunk->free = chunk->last;
	}
	if (!chunk || chunk->free + newSize > chunk->end) {
		u64 newChunkSize = chunk ? (chunk->end - (u8*)chunk) : TempMinChunkSize;
		do {
			newChunkSize *= 2;
		} while (newChunkSize < newSize + sizeof(TempChunk));
		JC_ASSERT(newChunkSize <= TempMaxChunkSize);
		TempChunk* const newChunk = tempAllocatorApiImpl.AllocChunk(newChunkSize);
		newChunk->next = chunk;
		chunk = newChunk;
	}
	chunk->last = chunk->free;
	chunk->free += newSize;
	if (ptr && ptr != chunk->last) {
		memcpy(chunk->last, ptr, Min(oldSize, newSize));
	}
	return chunk->last;
}

//--------------------------------------------------------------------------------------------------

struct TestFrame {
	Span<u64> allocs[5];
	u32       freeBeforeFrame[TempMaxChunkSizes];
	u32       freeAfterFrame[TempMaxChunkSizes];
};

constexpr u64 Sizes[TempMaxChunkSizes] = {
	             64 * (u64)1024 - sizeof(TempChunk),
	            128 * (u64)1024 - sizeof(TempChunk),
	            256 * (u64)1024 - sizeof(TempChunk),
	            512 * (u64)1024 - sizeof(TempChunk),
	           1024 * (u64)1024 - sizeof(TempChunk),
	       2 * 1024 * (u64)1024 - sizeof(TempChunk),
	       4 * 1024 * (u64)1024 - sizeof(TempChunk),
	       8 * 1024 * (u64)1024 - sizeof(TempChunk),
	      16 * 1024 * (u64)1024 - sizeof(TempChunk),
	      32 * 1024 * (u64)1024 - sizeof(TempChunk),
	      64 * 1024 * (u64)1024 - sizeof(TempChunk),
	     128 * 1024 * (u64)1024 - sizeof(TempChunk),
	     256 * 1024 * (u64)1024 - sizeof(TempChunk),
	     512 * 1024 * (u64)1024 - sizeof(TempChunk),
	    1024 * 1024 * (u64)1024 - sizeof(TempChunk),
	2 * 1024 * 1024 * (u64)1024 - sizeof(TempChunk),
	4 * 1024 * 1024 * (u64)1024 - sizeof(TempChunk),
};

TEST("TempAllocatorApi") {
	TempAllocatorApiImpl api;
	api.Init(VirtualMemoryApi::Get());

	SUBTEST("Allocs and reclaiming") {
		TempAllocatorImpl* ta[5];
		for (u32 i = 0; i < 5; i++) {
			ta[i] = (TempAllocatorImpl*)api.Create();
		}

		api.maxUnusedFrames = 2;
		TestFrame testFrames[] = {
			{
				.allocs = {
					{},
					{ Sizes[0] - sizeof(TempAllocatorImpl), 1, Sizes[1] - 1, 1, Sizes[2] - 1, Sizes[3] - 1, 1, 1 },
					{ Sizes[4], Sizes[5], Sizes[10], Sizes[12] + 1 },
					{ Sizes[3], Sizes[4] - 24, 8, 8, 8, Sizes[5] },
					{ Sizes[15] + 1, Sizes[14], Sizes[13], Sizes[12], Sizes[11], Sizes[10], Sizes[9], Sizes[8], Sizes[7], Sizes[6], Sizes[5], Sizes[4], Sizes[3], Sizes[2], Sizes[1], Sizes[0] },
				},
				.freeBeforeFrame = { 5, 1, 1, 2, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
				.freeAfterFrame  = { 5, 1, 1, 2, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
			},

			{
				.allocs = {},
				.freeBeforeFrame = { 5, 1, 1, 2, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
				.freeAfterFrame  = { 5, 1, 1, 2, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
			},

			{
				.allocs = {},
				.freeBeforeFrame = { 5, 1, 1, 2, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
				.freeAfterFrame  = { 5, 1, 1, 2, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
			},

			{
				.allocs = {},
				.freeBeforeFrame = { 5, 1, 1, 2, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
				.freeAfterFrame  = { 5, 1, 1, 1, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },	// frees [3]
			},

			{
				.allocs = {},
				.freeBeforeFrame = { 5, 1, 1, 1, 3, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
				.freeAfterFrame  = { 5, 1, 1, 1, 2, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },	// frees [4]
			},

			{
				.allocs = {},
				.freeBeforeFrame = { 5, 1, 1, 1, 2, 2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
				.freeAfterFrame  = { 5, 1, 1, 1, 2, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },	// frees [5]
			},

			{
				.allocs = {},
				.freeBeforeFrame = { 5, 1, 1, 1, 2, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
				.freeAfterFrame  = { 5, 1, 1, 1, 2, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1 },	// frees [6]
			},
		};

		for (const TestFrame* tf = testFrames; tf < testFrames + (sizeof(testFrames) / sizeof(testFrames[0])); tf++) {
			for (u32 a = 0; a < 5; a++) {
				for (u32 i = 0; i < tf->allocs[a].len; i++) {
					ta[a]->Alloc(tf->allocs[a][i], SrcLoc::Here());
				}
			}

			for (u32 i = 0; i < 5; i++) {
				api.Reset(ta[i]);
			}

			for (u32 i = 0; i < TempMaxChunkSizes; i++) {
				CHECK_EQ(api.freeLists[i].count, tf->freeBeforeFrame[i]);
			}

			api.Frame();

			for (u32 i = 0; i < TempMaxChunkSizes; i++) {
				CHECK_EQ(api.freeLists[i].count, tf->freeAfterFrame[i]);
			}
		}

		for (u32 i = 0; i < 5; i++) {
			api.Destroy(ta[i]);
		}
	}

	SUBTEST("Realloc") {
		TempAllocatorImpl* ta = (TempAllocatorImpl*)api.Create();
		u8* p1 = (u8*)ta->Alloc(1, SrcLoc::Here());
		u8* p2 = (u8*)ta->Realloc(p1, 1, 100, SrcLoc::Here());
		CHECK_EQ(p1, p2);
		u8* p3 = (u8*)ta->Realloc(p2, 100, 30, SrcLoc::Here());
		CHECK_EQ(p1, p2);

		for (u32 i = 0; i < 30; i++) { p3[i] = (u8)(100 + i); }
		u8* p4 = (u8*)ta->Alloc(1, SrcLoc::Here()); p4;
		u8* p5 = (u8*)ta->Realloc(p3, 30, 31, SrcLoc::Here());
		CHECK_NEQ(p3, p5);
		for (u32 i = 0; i < 30; i++) { CHECK_EQ(p3[i], p5[i]); }

		api.Destroy(ta);
	}

	SUBTEST("Create with buffer") {
		char buf[256];
		TempAllocatorImpl* ta = (TempAllocatorImpl*)api.Create(buf, sizeof(buf));

		void* p1 = ta->Alloc(sizeof(buf) - sizeof(TempChunk) - sizeof(TempAllocatorImpl), SrcLoc::Here());
		CHECK_EQ(p1, buf + sizeof(TempChunk) + sizeof(TempAllocatorImpl));
		void* p2 = ta->Alloc(1, SrcLoc::Here());
		CHECK_NEQ(p2, buf);

		api.Destroy(ta);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC