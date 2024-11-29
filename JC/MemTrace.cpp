#include "JC/MemTrace.h"

#include "JC/Array.h"
#include "JC/Map.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct MemTrace : MemTraceApi {
	struct Scope {
		s8  name     = {};
		u64 bytes    = 0;
		u32 allocs   = 0;
		u32 children = 0;
		u32 parent   = 0;
	};

	struct Trace {
		SrcLoc sl     = {};
		u64    bytes  = 0;
		u32    allocs = 0;
		u32    scope  = 0;
	};

	static constexpr u32 MaxScopes = 1024;
	Mem*            mem               = 0;
	Scope           scopes[MaxScopes] = {};
	u32             scopesLen         = 0;
	Array<Trace>    traces            = {};
	Map<u64, u64>   srcLocToTrace     = {};
	Map<void*, u64> ptrToTrace        = {};

	void Init(Mem* memIn) {
		mem = memIn;
	}

	MemScope CreateScope(s8 name, MemScope parent) {
		name;parent;
		return MemScope{};
	}

	void DestroyScope(MemScope scope) {
		scope;
	}

	void PermAlloc(u64 size) {
		size;
	}

	void AddTrace(MemScope scope, void* ptr, u64 size, SrcLoc sl) {
		scope;ptr;size;sl;
	}

	void RemoveTrace(MemScope scope, void* ptr) {
		scope;ptr;
	}
};

MemTrace memTraceApi;

MemTraceApi* MemTraceApi::Get() {
	return &memTraceApi;
}

/*
//--------------------------------------------------------------------------------------------------



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

	void RemoveTrace(void* ptr) {
		if (Opt<u64> idx = ptrToTrace.Find(ptr); idx.hasVal) {
			Trace* trace = &traces[idx.val];
			Assert(trace->allocs > 0);
			trace->allocs--;
			trace->bytes -= size;
			ptrToTrace.Remove(ptr);
		}
	}

	//----------------------------------------------------------------------------------------------

		traces.mem = &memObjs[0];
		srcLocToTrace.Init(&memObjs[0]);
		ptrToTrace.Init(&memObjs[0]);
		memLeakReporter = reporterIn;


			Mem* CreateScope(s8 name, Mem* parent) override {
		Assert(!parent || ((MemObj*)parent >= &memObjs[1] && (MemObj*)parent <= &memObjs[MaxMemObjs]));
		Assert(freeMemObjs);
		MemObj* const m = freeMemObjs;
		freeMemObjs = freeMemObjs->parent;
		m->name     = name;
		m->bytes    = 0;
		m->allocs   = 0;
		m->children = 0;
		m->parent   = (MemObj*)parent;
		if (m->parent) {
			m->parent->children++;
		}
		return m;
	}

	//----------------------------------------------------------------------------------------------

	void DestroyScope(Mem* mem) override {
		MemObj* m = (MemObj*)mem;
		if (
			memLeakReporter && 
			(
				(m->bytes > 0 || m->allocs > 0) ||
				(m->children > 0)
			)
		) {
			memLeakReporter->Begin(m->name, m->bytes, m->allocs, m->children);
			for (u64 i = 0; i < traces.len; i++) {
				if (traces[i].mem == m) {
					memLeakReporter->Alloc(traces[i].sl, traces[i].bytes, traces[i].allocs);
				}
			}
			if (m->children > 0) {
				for (u32 i = 0; i < MaxMemObjs; i++) {
					if (memObjs[i].parent == m) {
						memLeakReporter->Child(memObjs[i].name, memObjs[i].bytes, memObjs[i].allocs);
					}
				}
			}
		}
		MemSet(m, 0, sizeof(MemObj));
		m->parent = freeMemObjs;
		freeMemObjs = m;
	}
*/
//--------------------------------------------------------------------------------------------------

}	// namespace JC