#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"
#include "JC/Sys.h"
#include "JC/UnitTest.h"
#include <malloc.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct AllocatorImpl : Allocator {
	s8             name;
	u64            bytes    = 0;
	u32            allocs   = 0;
	u32            children = 0;
	AllocatorImpl* parent   = nullptr;

	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc srcLoc) override;
	void  Free(const void* ptr, u64 size) override;
};

//--------------------------------------------------------------------------------------------------

struct Trace {
	AllocatorImpl* allocator = nullptr;
	SrcLoc         srcLoc;
	u32            allocs = 0;
	u64            bytes = 0;
};

//--------------------------------------------------------------------------------------------------

struct AllocatorApiImpl : AllocatorApi {
	static constexpr u32 MaxAllocators  = 1024;

	MemLeakReporter*      memLeakReporter = nullptr;
	AllocatorImpl         allocators[MaxAllocators];
	AllocatorImpl*        freeAllocators = nullptr;	// parent is the "next" link field
	Array<Trace>          traces;
	Map<u64, u64>         srcLocToTrace;
	Map<const void*, u64> ptrToTrace;

	//----------------------------------------------------------------------------------------------

	void AddTrace(AllocatorImpl* a, const void* ptr, u64 size, SrcLoc srcLoc) {
		const u64 key = HashCombine(Hash(srcLoc.file), &srcLoc.line, sizeof(srcLoc.line));
		if (Opt<u64> idx = srcLocToTrace.Find(key)) {
			Trace* const trace = &traces[idx.val];
			trace->allocs++;
			trace->bytes += size;
			ptrToTrace.Put(ptr, idx.val);
		} else {
			traces.Add(Trace {
				.allocator = a,
				.srcLoc    = srcLoc,
				.allocs    = 1,
				.bytes     = size,
			});
			ptrToTrace.Put(ptr, traces.len - 1);
		}
	}

	//----------------------------------------------------------------------------------------------

	void RemoveTrace(const void* ptr, u64 size) {
		if (Opt<u64> idx = ptrToTrace.Find(ptr)) {
			Trace* const trace = &traces[idx];
			JC_ASSERT(trace->allocs > 0);
			trace->allocs--;
			trace->bytes -= size;
			ptrToTrace.Remove(ptr);
		}
	}

	//----------------------------------------------------------------------------------------------

	void* Alloc(AllocatorImpl* a, u64 size, SrcLoc srcLoc) {
		void* ptr = malloc(size);
		JC_ASSERT(ptr != nullptr);
		if (a == &allocators[0]) {
			return ptr;
		}
		AddTrace(a, ptr, size, srcLoc);
		return ptr;
	}

	//----------------------------------------------------------------------------------------------

	void* Realloc(AllocatorImpl* a, const void* ptr, u64 oldSize, u64 newSize, SrcLoc srcLoc) {
		if (ptr) {
			RemoveTrace(ptr, oldSize);
		}
		void* const newPtr = realloc((void*)ptr, newSize);
		JC_ASSERT(newPtr != nullptr);
		if (a == &allocators[0]) {
			return newPtr;
		}
		AddTrace(a, newPtr, newSize, srcLoc);
		return newPtr;
	}

	//----------------------------------------------------------------------------------------------

	void Free(const void* ptr, u64 size) {
		if (ptr) {
			RemoveTrace(ptr, size);
			free((void*)ptr);
		}
	}

	//----------------------------------------------------------------------------------------------

	void Init() {
		freeAllocators = &allocators[1];	// allocators[0] is reserved for internal allocations
		for (u32 i = 1; i < MaxAllocators - 1; i++) {
			allocators[i].parent = &allocators[i + 1];
		}
		allocators[MaxAllocators - 1].parent = nullptr;

		traces.Init(&allocators[0]);
		srcLocToTrace.Init(&allocators[0]);
		ptrToTrace.Init(&allocators[0]);
	}

	//----------------------------------------------------------------------------------------------

	Allocator* Create(s8 name, Allocator* parent) override {
		JC_ASSERT(!parent || ((AllocatorImpl*)parent >= &allocators[1] && (AllocatorImpl*)parent <= &allocators[MaxAllocators]));
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
						memLeakReporter->Alloc(traces[i].srcLoc.file, traces[i].srcLoc.line, traces[i].bytes, traces[i].allocs);
					}
				}
			}
			if (a->children > 0) {
				for (u32 i = 0; i < MaxAllocators; i++) {
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

void* AllocatorImpl::Alloc(u64 size, SrcLoc srcLoc) {
	return allocatorApiImpl.Alloc(this, size, srcLoc);
}

void* AllocatorImpl::Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc srcLoc) {
	return allocatorApiImpl.Realloc(this, ptr, oldSize, newSize, srcLoc);
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

	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc srcLoc) override;
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