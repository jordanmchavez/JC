#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Sys.h"
#include <malloc.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 Align(u64 size, u64 align) {
	return (size + align - 1) & ~(align - 1);
}

//--------------------------------------------------------------------------------------------------

struct AllocatorImpl : Allocator {
	s8             name;
	u64            bytes    = 0;
	u32            allocs   = 0;
	u32            children = 0;
	AllocatorImpl* parent   = nullptr;

	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(const void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) override;
	void  Free(const void* ptr, u64 size) override;
};

//--------------------------------------------------------------------------------------------------

struct TempChunk {
	u8*        free  = nullptr;
	u8*        end   = nullptr;
	u8*        last  = nullptr;
	TempChunk* next  = nullptr;
};
struct TempFreeList {
	TempChunk* chunks           = nullptr;
	u32        count            = 0;
	u32        unusedThisFrame  = 0;
	u32        framesWithUnused = 0;
};

static constexpr u64 TempMinChunkSizeLog2 = 16;	// 64k
static constexpr u64 TempMaxChunkSizeLog2 = 32;	// 4gb
static constexpr u64 TempMaxChunkSize     = 0xffffffff;
static constexpr u64 TempMaxAllocSize     = TempMaxChunkSize - sizeof(TempChunk);
static constexpr u64 TempSizeCount        = TempChunkSizeLog2Max - TempChunkSizeLog2Min;
static constexpr u64 TempAlign            = 8;

static_assert(sizeof(TempChunk) % TempAlign == 0);
static_assert(alignof(TempChunk) == TempAlign);


struct TempAllocatorApi {
	static constexpr TempChunk Empty;

	VirtualMemoryApi* virtualMemoryApi = nullptr;
	TempFreeList      freeLists[TempSizeCount];
	TempChunk*        frameChunk = nullptr;

	void Init(VirtualMemoryApi* inVirtualMemoryApi) {
		virtualMemoryApi = inVirtualMemoryApi;
	}

	TempAllocator* Frame() {
	}

	void* FrameAlloc(u64 size) {
	}

	void FrameRealloc(void* ptr, u64 oldSize, u64 newSize) {
	}

	TempChunk* AllocChunk(u64 size) {
		const u32 i = Log2(size);
		TempFreeList* const freeList = freeLists[i];
		TempChunk* chunk = freeList->chunks;
		if (chunk) {
			freeList->count--;
			freeList->unusedThisFrame = Min(freeList->unusedThisFrame, freeList->count);
		} else {
			chunk = (TempChunk*)virtualMemoryApi->Map(size);
		}
		chunk->free = (u8*)(chunk + 1);
		chunk->last = (u8*)(chunk + 1);
		chunk->end  = (u8*)chunk + size;
		return chunk;
	}

	void FreeChunk(TempChunk* chunk) {
	}
};

TempAllocatorApi tempAllocatorApiImpl;

//--------------------------------------------------------------------------------------------------

struct TempAllocatorImpl : TempAllocator {
	TempChunk* chunk = nullptr;

	void* Alloc(u64 size, SrcLoc srcLoc) override {
		size = Align(size, TempAlign);
		JC_ASSERT(size <= TempMaxAllocSize);

		if (!chunk || chunk->free + size > chunk->end) {
			u64 newChunkSize = chunk->end - (u8*)chunk;
			do {
				newChunkSize *= 2;
			} while (newChunkSize < size);
			JC_ASSERT(newChunkSize <= TempMaxChunkSize);
			TempChunk* const newChunk = tempAllocatorApiImpl.AllocChunk(newChunkSize);
			newChunk->next = chunk;
			chunk = newChunk;
		}

		chunk->last = chunk->free;
		chunk->free += size;
		return chunk->last;
	}

	void* Realloc(const void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) override {
	}

	void  Free(const void*, u64) override {}
};

struct FrameAllocator : TempAllocator {
	void* Alloc(u64 size, SrcLoc srcLoc) override {
		JC_ASSERT(size <= TempMaxSize);
		if (chunk) {
		} else {
			chunk =- 
		}
		tempAllocatorApiImpl
	}

	void* Realloc(const void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) override {
		tempAllocatorApiImpl
	}

	void  Free(const void*, u64) override {}
};

//--------------------------------------------------------------------------------------------------

struct AllocTrace {
	AllocatorImpl* allocator = nullptr;
	SrcLoc         srcLoc;
	u32            allocs    = 0;
	u64            bytes     = 0;
};

struct AllocatorApiImpl : AllocatorApi {
	static constexpr u32 MaxAllocators  = 1024;
	static constexpr u64 TempAllocAlign = 16;
	static constexpr u64 TempChunkSize  = 16 * 1024 * 1024;

	VirtualMemoryApi*     virtualMemoryApi = nullptr;
	MemLeakReporter*      memLeakReporter = nullptr;
	AllocatorImpl         allocators[MaxAllocators];
	AllocatorImpl*        freeAllocators = nullptr;	// parent is the "next" link field
	Array<AllocTrace>     traces;
	Map<u64, u64>         keyToTrace;
	Map<const void*, u64> ptrToTrace;

	u8* AddTempChunk(u64 size) {
		TempChunk* tempChunk = (TempChunk*)virtualMemoryApi->Map(size);
		JC_ASSERT(tempChunk);
		tempChunk->size = size;
		tempChunk->next = tempChunks;
		tempChunks = tempChunk;
		return (u8*)(tempChunk + 1);
	}

	void Init(LogApi* inLogApi, VirtualMemoryApi* inVirtualMemoryApi) {
		logApi = inLogApi;
		virtualMemoryApi = inVirtualMemoryApi;

		freeAllocators = &allocators[1];	// allocators[0] is reserved for internal allocations
		for (u64 i = 1; i < MaxAllocators - 1; i++) {
			allocators[i].parent = &allocators[i + 1];
		}
		allocators[MaxAllocators - 1].parent = nullptr;

		traces.Init(&allocators[0]);
		keyToTrace.Init(&allocators[0]);
		ptrToTrace.Init(&allocators[0]);

		tempChunks    = nullptr;
		tempBegin     = AddTempChunk(TempChunkSize);
		tempEnd       = tempBegin + TempChunkSize - sizeof(TempChunk);
		tempLastAlloc = nullptr;
	}

	void Shutdown() {
		traces.Shutdown();
		keyToTrace.Shutdown();
		ptrToTrace.Shutdown();

		ResetTemp();
		virtualMemoryApi->Unmap(tempChunks, tempChunks->size);
	}

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

	all temp allocators should use the 1024 local trick;
	/*
	Options:
	1. pass logapi to Destroy
		exposes the fact that w're coupling two things
	2. separate temp allocator into its own interface
	3. have Detroy return an error structure
	4. separate the leak check into a dedicated function that either accepts logapi or returns leak stats
	*/
	void Destroy(Allocator* allocator) override {
		AllocatorImpl* const a = (AllocatorImpl*)allocator;
		if (memLeakReporter) {
			memLeakReporter->Alloc(a->name, a->bytes, a->allocs, a->children);
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

	void SetMemLeakReporter(MemLeakReporter* reporter) override {
		memLeakReporter = reporter;
	}

	TempAllocator* Temp() override {
		return &tempAllocatorImpl;
	}

	void ResetTemp() override {
		TempChunk* chunk = tempChunks;
		while (chunk->next) {
			TempChunk* next = chunk->next;
			virtualMemoryApi->Unmap(chunk, chunk->size);
			chunk = next;
		}
		tempBegin     = (u8*)(chunk + 1);
		tempEnd       = tempBegin + chunk->size;
		tempLastAlloc = nullptr;
	}

	inline u64 Key(SrcLoc srcLoc, AllocatorImpl* a) {
		u64 key = Hash(srcLoc.file);
		key = HashCombine(key, &srcLoc.line, sizeof(srcLoc.line));
		const u32 index = (u32)(a - allocators);
		key = HashCombine(key, &index, sizeof(index));
		return key;
	}

	void Trace(AllocatorImpl* a, const void* ptr, u64 size, SrcLoc srcLoc) {
		AllocTrace* trace = 0;
		u64* idx = keyToTrace.Find(Key(srcLoc, a)).Or(nullptr);
		if (!idx) {
			trace = traces.Add(AllocTrace {
				.allocator = a,
				.srcLoc    = srcLoc,
				.allocs    = 1,
				.bytes     = size,
			});
			ptrToTrace.Put(ptr, traces.len - 1);
		} else {
			trace = &traces[*idx];
			trace->allocs++;
			trace->bytes += size;
		}
	}

	void Untrace(const void* ptr, u64 size) {
		u64* const idx = ptrToTrace.Find(ptr).Or(0);
		if (idx) {
			AllocTrace* trace = &traces[*idx];
			JC_ASSERT(trace->allocs > 0);
			JC_ASSERT(trace->bytes >= size);
			trace->allocs--;
			trace->bytes -= size;
			ptrToTrace.Remove(ptr);
		}
	}

	void* Alloc(AllocatorImpl* a, u64 size, SrcLoc srcLoc) {
		void* ptr = malloc(size);
		JC_ASSERT(ptr != nullptr);
		if (a == &allocators[0]) {
			return ptr;
		}
		Trace(a, ptr, size, srcLoc);
		return ptr;
	}

	void* Realloc(AllocatorImpl* a, const void* oldPtr, u64 oldSize, u64 newSize, SrcLoc srcLoc) {
		void* const newPtr = realloc((void*)oldPtr, newSize);
		JC_ASSERT(newPtr != nullptr);
		if (a == &allocators[0]) {
			return newPtr;
		}
		Untrace(oldPtr, oldSize);
		Trace(a, newPtr, newSize, srcLoc);
		return newPtr;
	}

	void Free(AllocatorImpl* a, const void* ptr, u64 size) {
		free((void*)ptr);
		if (a == &allocators[0]) {
			return;
		}
		Untrace(ptr, size);
	}

	void* TempAlloc(u64 size) {
		JC_ASSERT(size > 0);
		size = Align(size, TempAllocAlign);
		u8* ptr = nullptr;
		if (size <= (u64)(tempEnd - tempBegin)) {
			ptr = tempBegin;
			tempBegin += size;
			tempLastAlloc= ptr;

		} else if (size < TempChunkSize - sizeof(TempChunk)) {
			ptr = AddTempChunk(TempChunkSize);
			tempBegin = ptr + size;
			tempEnd  = ptr + TempChunkSize - sizeof(TempChunk);
			tempLastAlloc = ptr;
		
		} else {
			ptr = AddTempChunk(size + sizeof(TempChunk));
		}
		return (void*)Align((u64)ptr, TempAllocAlign);
	}

	void* TempRealloc(const void* ptr, u64 size, u64 newSize) {
		JC_ASSERT(newSize > 0);
		if (ptr != nullptr && ptr == tempLastAlloc && tempLastAlloc + newSize < tempEnd) {
			tempBegin = tempLastAlloc + newSize;
			return (void*)ptr;
		}
		void* res = TempAlloc(newSize);
		memcpy(res, ptr, size);
		return res;
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

void* AllocatorImpl::Realloc(const void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) {
	return allocatorApiImpl.Realloc(this, ptr, size, newSize, srcLoc);
}

void AllocatorImpl::Free(const void* ptr, u64 size) {
	allocatorApiImpl.Free(this, ptr, size);
}
//--------------------------------------------------------------------------------------------------

void* TempAllocatorImpl::Alloc(u64 size, SrcLoc) {
	return allocatorApiImpl.TempAlloc(size);
}

void* TempAllocatorImpl::Realloc(const void* ptr, u64 size, u64 newSize, SrcLoc) {
	return allocatorApiImpl.TempRealloc(ptr, size, newSize);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC