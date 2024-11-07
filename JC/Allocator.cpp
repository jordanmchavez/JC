#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Sys.h"
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
	void* Realloc(const void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) override;
	void  Free(const void* ptr, u64 size) override;
};

struct TempAllocatorImpl : TempAllocator {
	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(const void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) override;
	void  Free(const void*, u64) override {}
};

struct AllocTrace {
	AllocatorImpl* allocator = nullptr;
	SrcLoc         srcLoc;
	u32            allocs    = 0;
	u64            bytes     = 0;
};

struct TempChunk {
	u64        size;
	TempChunk* next;
};

struct AllocatorApiImpl : AllocatorApi {
	static constexpr u32 MaxAllocators  = 1024;
	static constexpr u64 TempAllocAlign = 16;
	static constexpr u64 TempChunkSize  = 16 * 1024 * 1024;

	LogApi*               logApi = nullptr;
	VirtualMemoryApi*     virtualMemoryApi = nullptr;
	AllocatorImpl         allocators[MaxAllocators];

	AllocatorImpl*        freeAllocators = nullptr;	// parent is the "next" link field
	Array<AllocTrace>     traces;
	Map<u64, u64>         keyToTrace;
	Map<const void*, u64> ptrToTrace;

	TempAllocatorImpl     tempAllocatorImpl;
	TempChunk*            tempChunks = nullptr;
	u8*                   tempBegin = nullptr;
	u8*                   tempEnd = nullptr;
	u8*                   tempLastAlloc = nullptr;

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
		AllocatorImpl* const sa = freeAllocators;
		freeAllocators = freeAllocators->parent;
		sa->name     = name;
		sa->bytes    = 0;
		sa->allocs   = 0;
		sa->children = 0;
		sa->parent   = (AllocatorImpl*)parent;
		if (sa->parent) {
			sa->parent->children++;
		}

		return sa;
	}

	void Destroy(Allocator* allocator) override {
		JC_ASSERT(allocator);

		AllocatorImpl* const sa = (AllocatorImpl*)allocator;
		if (sa->bytes > 0 || sa->allocs > 0) {
			JC_LOG_ERROR("Scope has unfreed allocs: name={}, bytes={}, allocs={}", sa->name, sa->bytes, sa->allocs);
			for (u64 i = 0; i < traces.len; i++) {
				if (traces[i].allocator == sa) {
					JC_LOG_ERROR("  {}({}): bytes={} allocs={}", traces[i].srcLoc.file, traces[i].srcLoc.line, traces[i].bytes, traces[i].allocs);
				}
			}
		}
		if (sa->children > 0) {
			JC_LOG_ERROR("Scope has open child scopes: name={}, children={}", sa->name, sa->children);
			for (u32 i = 0; i < MaxAllocators; i++) {
				if (allocators[i].parent == sa) {
					JC_LOG_ERROR("  Unclosed scope: name={}, bytes={}, allocs={}", allocators[i].name, allocators[i].bytes, allocators[i].allocs);
				}
			}
		}

		memset(sa, 0, sizeof(AllocatorImpl));
		sa->parent = freeAllocators;
		freeAllocators = sa;
	}

	TempAllocator* Temp() override {
		return &tempAllocatorImpl;
	}

	void ResetTemp() override {
		
	}

	inline u64 Key(SrcLoc srcLoc, AllocatorImpl* sa) {
		u64 key = Hash(srcLoc.file);
		key = HashCombine(key, &srcLoc.line, sizeof(srcLoc.line));
		const u32 index = (u32)(sa - allocators);
		key = HashCombine(key, &index, sizeof(index));
		return key;
	}

	void Trace(AllocatorImpl* sa, const void* ptr, u64 size, SrcLoc srcLoc) {
		AllocTrace* trace = 0;
		u64* idx = keyToTrace.Find(Key(srcLoc, sa)).Or(0);
		if (!idx) {
			trace = traces.Add(AllocTrace {
				.allocator = sa,
				.srcLoc    = srcLoc,
				.allocs    = 1,
				.bytes     = size,
			});
		} else {
			trace = &traces[*idx];
			trace->allocs++;
			trace->bytes += size;
		}
		ptrToTrace.Put(ptr, *idx);
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

	void* Alloc(AllocatorImpl* sa, u64 size, SrcLoc srcLoc) {
		void* ptr = malloc(size);
		JC_ASSERT(ptr != nullptr);

		if (sa == &allocators[0]) {
			return ptr;
		}

		Trace(sa, ptr, size, srcLoc);
	
		return ptr;
	}

	void* Realloc(AllocatorImpl* sa, const void* oldPtr, u64 oldSize, u64 newSize, SrcLoc srcLoc) {
		void* const newPtr = realloc((void*)oldPtr, newSize);
		JC_ASSERT(newPtr != nullptr);
		if (sa == &allocators[0]) {
			return newPtr;
		}
		Untrace(oldPtr, oldSize);
		Trace(sa, newPtr, newSize, srcLoc);
		return newPtr;
	}

	void Free(AllocatorImpl* sa, const void* ptr, u64 size) {
		free((void*)ptr);
		if (sa == &allocators[0]) {
			return;
		}
		Untrace(ptr, size);
	}

	static u64 Align(u64 size, u64 align) {
		return (size + align - 1) & ~(align - 1);
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