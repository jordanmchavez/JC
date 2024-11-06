#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Map.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct ScopedAllocator : Allocator {
	s8               name;
	u64              bytes;
	u32              allocs;
	u32              children;
	ScopedAllocator* parent;

	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) override;
	void  Free(void* ptr, u64 size) override;
};

//--------------------------------------------------------------------------------------------------

struct AllocTrace {
	ScopedAllocator* scope;
	SrcLoc           srcLoc;
	u32              allocs;
	u64              bytes;
};

struct AllocatorApiImpl : AllocatorApi {
	static constexpr u32  MaxScopedAllocators = 1024;

	ScopedAllocator       scopedAllocators[MaxScopedAllocators];
	u32                   scopedAllocatorsLen;
	Link*                 freeScopedAllocators;
	Array<AllocTrace>     traces;
	Map<u64, u64>         keyToTrace;
	Map<const void*, u64> ptrToTrace;

	void Init() {
		// scopedAllocators[0] is reserved for internal allocations
		scopedAllocatorsLen  = 1;
		freeScopedAllocators = nullptr;
		traces.Init(&scopedAllocators[0]);
		keyToTrace.Init(&scopedAllocators[0]);
		ptrToTrace.Init(&scopedAllocators[0]);
	}

	void Shutdown() {
		traces.Shutdown();
		keyToTrace.Shutdown();
		ptrToTrace.Shutdown();
	}

	Allocator* Create(s8 name, Allocator* parent) override {
		JC_ASSERT(!parent || (ScopedAllocator*)parent >= &scopedAllocators[1] && (ScopedAllocator*)parent <= &scopedAllocators[MaxScopedAllocators]);

		ScopedAllocator* sa = 0;
		if (freeScopedAllocators) {
			sa = (ScopedAllocator*)freeScopedAllocators;
			freeScopedAllocators = freeScopedAllocators->next;
		} else {
			JC_ASSERT(scopedAllocatorsLen < MaxScopedAllocators);
			sa = &scopedAllocators[scopedAllocatorsLen];
			scopedAllocatorsLen++;
		}

		sa->name     = name;
		sa->bytes    = 0;
		sa->allocs   = 0;
		sa->children = 0;
		sa->parent   = (ScopedAllocator*)parent;
		if (parent) {
			((ScopedAllocator*)parent)->children++;
		}

		return sa;
	}

	void Destroy(Allocator* allocator) override {
		ScopedAllocator* const sa = (ScopedAllocator*)allocator;
		if (sa->bytes > 0 || sa->allocs > 0) {
			Log_Err("Scope has unfreed allocs: name={}, bytes={}, allocs={}", sa->name, sa->bytes, sa->allocs);
			for (u64 i = 0; i < traces.len; i++) {
				if (traces[i].scope == sa) {
					Log_Err("  {}({}): bytes={} allocs={}", traces[i].srcLoc.file, traces[i].srcLoc.line, traces[i].bytes, traces[i].allocs);
				}
			}
		}
		if (sa->children > 0) {
			Log_Err("Scope has open child scopes: name={}, children={}", sa->name, sa->children);
			for (u32 i = 0; i < scopedAllocatorsLen; i++) {
				if (scopedAllocators[i].parent == sa) {
					Log_Err("  Unclosed scope: name={}, bytes={}, allocs={}", scopedAllocators[i].name, scopedAllocators[i].bytes, scopedAllocators[i].allocs);
				}
			}
		}

		memset(sa, 0, sizeof(ScopedAllocator));
		((Link*)sa)->next = freeScopedAllocators;
		freeScopedAllocators = (Link*)sa;
	}

	inline u64 Key(SrcLoc srcLoc, ScopedAllocator* sa) {
		u64 key = Hash(srcLoc.file);
		key = HashCombine(key, &srcLoc.line, sizeof(srcLoc.line));
		const u32 index = (u32)(sa - scopedAllocators);
		key = HashCombine(key, &index, sizeof(index));
		return key;
	}

	void Trace(ScopedAllocator* sa, const void* ptr, u64 size, SrcLoc srcLoc) {
		AllocTrace* trace = 0;
		u64* idx = keyToTrace.Find(Key(srcLoc, sa)).Or(0);
		if (!idx) {
			trace = traces.Add(AllocTrace { .scope = sa, .srcLoc = srcLoc, .allocs = 1, .bytes = size });
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
			Assert(trace->allocs > 0);
			Assert(trace->bytes >= size);
			trace->allocs--;
			trace->bytes -= size;
			ptrToTrace.Remove(ptr);
		}
	}

	void* Alloc(ScopedAllocator* sa, u64 size, SrcLoc srcLoc) {
		void* ptr = malloc(size);
		Assert(ptr != nullptr);

		if (sa == &scopedAllocators[0]) {
			return ptr;
		}

		threadApi->LockMutex(mutex);
		Trace(sa, ptr, size, srcLoc);
		threadApi->UnlockMutex(mutex);
	
		return ptr;
	}

	void* Realloc(ScopedAllocator* sa, void* oldPtr, u64 oldSize, u64 newSize, SrcLoc srcLoc) {
		void* const newPtr = realloc(oldPtr, newSize);
		Assert(newPtr != nullptr);

		if (sa == &scopedAllocators[0]) {
			return newPtr;
		}

		threadApi->LockMutex(mutex);
		Untrace(oldPtr, oldSize);
		Trace(sa, newPtr, newSize, srcLoc);
		threadApi->UnlockMutex(mutex);

		return newPtr;
	}

	void Free(ScopedAllocator* sa, void* ptr, u64 size) {
		free(ptr);

		if (sa == &scopedAllocators[0]) {
			return;
		}

		threadApi->LockMutex(mutex);
		Untrace(ptr, size);
		threadApi->UnlockMutex(mutex);
	}
};

static AllocatorApiImpl allocatorApiImpl;

AllocatorApi* AllocatorApi::Get() {
	return &allocatorApiImpl;
}

//--------------------------------------------------------------------------------------------------

void* ScopedAllocator::Alloc(u64 size, SrcLoc srcLoc) {
	return allocatorApiImpl.Alloc(this, size, srcLoc);
}

void* ScopedAllocator::Realloc(void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) {
	return allocatorApiImpl.Realloc(this, ptr, size, newSize, srcLoc);
}

void ScopedAllocator::Free(void* ptr, u64 size) {
	allocatorApiImpl.Free(this, ptr, size);
}

struct Link {
	Link* next;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC