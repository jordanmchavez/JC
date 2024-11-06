#include ""

//-------------------------------------------------------------------------------------------------

static u64 Align(u64 size, u64 align) {
	return (size + align - 1) & ~(align - 1);
}

//-------------------------------------------------------------------------------------------------

struct AllocatorApiImpl : AllocatorApi {

	void Init(LogApi* inLogApi, ThreadApi* inThreadApi) {
		logApi    = inLogApi;
		threadApi = inThreadApi;
		mutex = threadApi->CreateMutex();
		// reserve scoped allocator 0 for allocating AllocatorApi structures
		scopedAllocatorsLen = 1;
		freeScopedAllocators = nullptr;
		Allocator* a = &scopedAllocators[0];
		traces.Init(a);
		keyToTrace.Init(a);
		ptrToTrace.Init(a);
	}

	void Shutdown() {
		threadApi->DestroyMutex(mutex);
		scopedAllocatorsLen = 0;
		freeScopedAllocators = nullptr;
		traces.Shutdown();
		keyToTrace.Shutdown();
		ptrToTrace.Shutdown();
	}

	Allocator* Create(s8 name, Allocator* parent) override {
		threadApi->LockMutex(mutex);
		ScopedAllocator* sa = 0;
		if (freeScopedAllocators) {
			sa = (ScopedAllocator*)freeScopedAllocators;
			freeScopedAllocators = freeScopedAllocators->next;
		} else {
			Assert(scopedAllocatorsLen < MaxScopedAllocators);
			sa = &scopedAllocators[scopedAllocatorsLen];
			scopedAllocatorsLen++;
		}
		threadApi->UnlockMutex(mutex);

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

AllocatorApi* AllocatorApi::Init(LogApi* logApi, ThreadApi* threadApi) {
	allocatorApiImpl.Init(logApi, threadApi);
	return &allocatorApiImpl;
}

void AllocatorApi::Shutdown() {
	allocatorApiImpl.Shutdown();
}

//-------------------------------------------------------------------------------------------------

struct TempAllocator : Allocator {
	void* Alloc(u64 size, SrcLoc srcLoc) override;
	void* Realloc(void* ptr, u64 size, u64 newSize, SrcLoc srcLoc) override;
	void  Free(void*, u64) override {}
};

static constexpr u64 AllocAlign = 16;
static constexpr u64 ChunkSize  = 16 * 1024 * 1024;

struct Chunk {
	u64    size;
	Chunk* next;
};

struct TempAllocatorApiImpl : TempAllocatorApi {
	VirtualMemApi* virtualMemApi;
	Chunk*         chunks;
	u8*            begin;
	u8*            end;
	u8*            last;
	TempAllocator  tempAllocator;

	void Init(VirtualMemApi* inVirtualMemApi) {
		virtualMemApi = inVirtualMemApi;
		chunks        = 0;
		begin         = AllocChunk(ChunkSize);
		end           = begin + ChunkSize - sizeof(Chunk);
		last          = 0;
	}

	void Shutdown() {
		Reset();
		virtualMemApi->Unmap(chunks, chunks->size);
		chunks = 0;
		begin = end = last = 0;
	}
	u8* AllocChunk(u64 size) {
		Chunk* chunk = (Chunk*)virtualMemApi->Map(size);
		Assert(chunk != nullptr);
		chunk->size = size;
		chunk->next = chunks;
		chunks = chunk;
		return (u8*)(chunks + 1);
	}

	void* Alloc(u64 size) {
		Assert(size > 0);
		size = Align(size, AllocAlign);
		u8* ptr = nullptr;
		if (size <= (u64)(end - begin)) {
			ptr = begin;
			begin += size;
			last = ptr;

		} else if (size < ChunkSize - sizeof(Chunk)) {
			ptr = AllocChunk(ChunkSize);
			begin = ptr + size;
			end  = ptr + ChunkSize - sizeof(Chunk);
			last = ptr;
		
		} else {
			ptr = AllocChunk(size + sizeof(Chunk));
		}
		return (void*)Align((u64)ptr, AllocAlign);
	}

	void* Realloc(void* ptr, u64 size, u64 newSize) {
		Assert(newSize > 0);
		if (ptr != nullptr && ptr == last && last + newSize < end) {
			begin = last + newSize;
			return ptr;
		}

		void* res = Alloc(newSize);
		memcpy(res, ptr, size);
		return res;
	}

	Allocator* Get() override {
		return &tempAllocator;
	}

	void Reset() override {
		Chunk* chunk = chunks;
		while (chunk->next) {
			Chunk* next = chunk->next;
			virtualMemApi->Unmap(chunk, chunk->size);
			chunk = next;
		}
		begin = (u8*)(chunk + 1);
		end = begin + chunk->size;
		last = 0;
	}
};

static TempAllocatorApiImpl tempAllocatorApiImpl;

TempAllocatorApi* TempAllocatorApi::Init(VirtualMemApi* virtualMemApi) {
	tempAllocatorApiImpl.Init(virtualMemApi);
	return &tempAllocatorApiImpl;
}

void TempAllocatorApi::Shutdown() {
	tempAllocatorApiImpl.Shutdown();
}

//-------------------------------------------------------------------------------------------------


void* TempAllocator::Alloc(u64 size, SrcLoc) {
	return tempAllocatorApiImpl.Alloc(size);
}

void* TempAllocator::Realloc(void* ptr, u64 size, u64 newSize, SrcLoc) {
	return tempAllocatorApiImpl.Realloc(ptr, size, newSize);
}

//-------------------------------------------------------------------------------------------------

}	// namespace JC