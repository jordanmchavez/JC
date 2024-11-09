#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"
#include "JC/Sys.h"
#include <malloc.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct AllocatorImpl : Allocator {
	s8             name;
	u64            bytes;
	u32            allocs;
	u32            children;
	AllocatorImpl* parent;

	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc srcLoc) override;
	void  Free(const void* ptr, u64 size) override;
};

//--------------------------------------------------------------------------------------------------

struct AllocTrace {
	AllocatorImpl* allocator = nullptr;
	SrcLoc         srcLoc;
	u32            allocs    = 0;
	u64            bytes     = 0;
};

//--------------------------------------------------------------------------------------------------

struct AllocatorApiImpl : AllocatorApi {
	static constexpr u32 MaxAllocators  = 1024;

	MemLeakReporter*      memLeakReporter = nullptr;
	AllocatorImpl         allocators[MaxAllocators];
	AllocatorImpl*        freeAllocators = nullptr;	// parent is the "next" link field
	Array<AllocTrace>     traces;
	Map<u64, u64>         keyToTrace;
	Map<const void*, u64> ptrToTrace;

	//----------------------------------------------------------------------------------------------

	inline u64 TraceKey(SrcLoc srcLoc, AllocatorImpl* a) {
		u64 key = Hash(srcLoc.file);
		key = HashCombine(key, &srcLoc.line, sizeof(srcLoc.line));
		const u32 index = (u32)(a - allocators);
		key = HashCombine(key, &index, sizeof(index));
		return key;
	}

	//----------------------------------------------------------------------------------------------

	void Trace(AllocatorImpl* a, const void* ptr, u64 size, SrcLoc srcLoc) {
		AllocTrace* trace = 0;
		u64* idx = keyToTrace.Find(TraceKey(srcLoc, a)).Or(nullptr);
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

	//----------------------------------------------------------------------------------------------

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

	//----------------------------------------------------------------------------------------------

	void* Alloc(AllocatorImpl* a, u64 size, SrcLoc srcLoc) {
		void* ptr = malloc(size);
		JC_ASSERT(ptr != nullptr);
		if (a == &allocators[0]) {
			return ptr;
		}
		Trace(a, ptr, size, srcLoc);
		return ptr;
	}

	//----------------------------------------------------------------------------------------------

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

	//----------------------------------------------------------------------------------------------

	void Free(AllocatorImpl* a, const void* ptr, u64 size) {
		free((void*)ptr);
		if (a == &allocators[0]) {
			return;
		}
		Untrace(ptr, size);
	}

	//----------------------------------------------------------------------------------------------

	void Init() {
		freeAllocators = &allocators[1];	// allocators[0] is reserved for internal allocations
		for (u64 i = 1; i < MaxAllocators - 1; i++) {
			allocators[i].parent = &allocators[i + 1];
		}
		allocators[MaxAllocators - 1].parent = nullptr;

		traces.Init(&allocators[0]);
		keyToTrace.Init(&allocators[0]);
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
	allocatorApiImpl.Free(this, ptr, size);
}
//--------------------------------------------------------------------------------------------------

struct TempChunk {
	u8*        end;
	u8*        free;
	u8*        last;
	TempChunk* next;
};

struct TempFreeList {
	TempChunk* chunks;
	u32        count;
	u32        minCountThisFrame;
	u32        framesWithUnused;
};

//--------------------------------------------------------------------------------------------------

struct TempAllocatorImpl : TempAllocator {
	TempChunk* chunk;
	void*      buf;

	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(const void* ptr, u64 oldSize, u64 newSize, SrcLoc srcLoc) override;
};

//--------------------------------------------------------------------------------------------------

struct TempAllocatorApiImpl : TempAllocatorApi {
	static constexpr u64 MinChunkSizeLog2 = 16;	// 64k
	static constexpr u64 MaxChunkSizeLog2 = 32;	// 4gb
	static constexpr u64 MaxChunkSize     = 0xffffffff;
	static constexpr u64 MaxAllocSize     = MaxChunkSize - sizeof(TempChunk);
	static constexpr u64 SizeCount        = MaxChunkSizeLog2 - MinChunkSizeLog2;
	static constexpr u64 AllocAlign       = 8;

	static_assert(sizeof(TempChunk) % AllocAlign == 0);
	static_assert(alignof(TempChunk) == AllocAlign);

	static_assert(sizeof(TempAllocatorImpl) % AllocAlign == 0);
	static_assert(alignof(TempAllocatorImpl) == AllocAlign);

	VirtualMemoryApi* virtualMemoryApi;
	TempFreeList      freeLists[SizeCount];
	u32               nextSizeToCheck;

	//----------------------------------------------------------------------------------------------

	TempChunk* AllocChunk(u64 size) {
		const u32 i = Log2(size);
		TempFreeList* const freeList = &freeLists[i];
		TempChunk* chunk = freeList->chunks;
		if (chunk) {
			freeList->count--;
			freeList->minCountThisFrame = Min(freeList->minCountThisFrame, freeList->count);
		} else {
			chunk = (TempChunk*)virtualMemoryApi->Map(size);
		}
		chunk->end  = (u8*)chunk + size;
		chunk->free = (u8*)(chunk + 1);
		chunk->last = (u8*)(chunk + 1);
		chunk->next = nullptr;
		return chunk;
	}

	//----------------------------------------------------------------------------------------------

	void FreeChunk(TempChunk* chunk) {
		TempFreeList* const freeList = &freeLists[Log2(chunk->end - (u8*)chunk) - MinChunkSizeLog2];
		chunk->next = freeList->chunks;
		freeList->chunks = chunk;
		freeList->count++;
	}

	//----------------------------------------------------------------------------------------------

	void Init(VirtualMemoryApi* inVirtualMemoryApi) override {
		virtualMemoryApi = inVirtualMemoryApi;
		nextSizeToCheck = 0;
	}

	//----------------------------------------------------------------------------------------------
	
	TempAllocator* Create() override {
		TempChunk* const chunk = AllocChunk(1 << MinChunkSizeLog2);

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
		while (chunk) {
			TempChunk* next = chunk->next;
			if (chunk != ((TempAllocatorImpl*)ta)->buf) {
				FreeChunk(chunk);
			}
			chunk = next;
		}
	}

	//----------------------------------------------------------------------------------------------

	void Frame() {
		for (u32 i = 0; i < SizeCount; i++) {
			freeLists[i].framesWithUnused = freeLists[i].minCountThisFrame ? freeLists[i].framesWithUnused + 1 : 0;
			freeLists[i].minCountThisFrame = freeLists[i].count;
		}
		TempFreeList* check = &freeLists[nextSizeToCheck];
		if (check->framesWithUnused > 30) {
			TempChunk* chunk = check->chunks;
			JC_ASSERT(chunk);
			check->chunks = chunk->next;
			check->count--;
			check->minCountThisFrame = Min(check->minCountThisFrame, check->count);
			virtualMemoryApi->Unmap(chunk, chunk->end - (u8*)chunk);
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
	size = Align(size, TempAllocatorApiImpl::AllocAlign);
	JC_ASSERT(size <= TempAllocatorApiImpl::MaxAllocSize);
	if (!chunk || chunk->free + size > chunk->end) {
		u64 newChunkSize = chunk->end - (u8*)chunk;
		do {
			newChunkSize *= 2;
		} while (newChunkSize < size);
		JC_ASSERT(newChunkSize <= TempAllocatorApiImpl::MaxChunkSize);
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
	newSize = Align(newSize, TempAllocatorApiImpl::AllocAlign);
	JC_ASSERT(newSize <= TempAllocatorApiImpl::MaxAllocSize);
	if (chunk && chunk->last == ptr) {
		chunk->free = chunk->last;
	}
	if (!chunk || chunk->free + newSize > chunk->end) {
		u64 newChunkSize = chunk->end - (u8*)chunk;
		do {
			newChunkSize *= 2;
		} while (newChunkSize < newSize);
		JC_ASSERT(newChunkSize <= TempAllocatorApiImpl::MaxChunkSize);
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

}	// namespace JC