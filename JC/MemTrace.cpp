#include "JC/MemTrace.h"

#include "JC/Array.h"
#include "JC/Map.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct MemTrace : MemTraceApi {
	struct Scope {
		s8     name     = {};
		u64    bytes    = 0;
		u32    allocs   = 0;
		u32    children = 0;
		Scope* parent   = 0;	// doubles as next in free list
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
	Scope*          freeScopes        = 0;
	Array<Trace>    traces            = {};
	Map<u64, u64>   slToTrace         = {};
	Map<void*, u64> ptrToTrace        = {};

	void Init() {
		traces.mem = Sys::VirtualMem();
		slToTrace.Init(Sys::VirtualMem());
		ptrToTrace.Init(Sys::VirtualMem());
		scopes[0] = {};
		for (u32 i = 1; i < MaxScopes - 1; i++) {
			scopes[i].parent = &scopes[i + 1];
		}
		scopes[MaxScopes - 1].parent = 0;
		freeScopes = &scopes[1];	// reserve index 0 for invalid
	}

	MemScope CreateScope(s8 name, MemScope parent) {
		Assert(parent.opaque > 0 && parent.opaque < MaxScopes);
		Assert(freeScopes);
		Scope* const scope = freeScopes;
		freeScopes = freeScopes->parent;
		scope->name     = name;
		scope->bytes    = 0;
		scope->allocs   = 0;
		scope->children = 0;
		scope->parent   = &scopes[parent.opaque];
		if (scope->parent) {
			scope->parent->children++;
		}
		return MemScope { .opaque = (u64)(scope - scopes); };
	}

	void DestroyScope(MemScope memScope) {
		Assert(memScope.opaque > 0 && memScope.opaque < MaxScopes);
		Scope* scope = &scopes[memScope.opaque];
		if (scope->bytes > 0 || scope->allocs > 0) {
			memLeakReporter->Begin(scope->name, scope->bytes, scope->allocs, scope->children);

			for (u64 i = 0; i < traces.len; i++) {
				if (traces[i].mem == scope) {
					memLeakReporter->Alloc(traces[i].sl, traces[i].bytes, traces[i].allocs);
				}
			}
			if (scope->children > 0) {
				for (u32 i = 0; i < MaxMemObjs; i++) {
					if (memObjs[i].parent == scope) {
						memLeakReporter->Child(memObjs[i].name, memObjs[i].bytes, memObjs[i].allocs);
					}
				}
			}
		}
		MemSet(scope, 0, sizeof(MemObj));
		scope->parent = freeMemObjs;
		freeMemObjs = scope;

	}

	void PermAlloc(u64 size) {
		size;
	}

	void AddTrace(MemScope scope, void* ptr, u64 size, SrcLoc sl) {
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

	void RemoveTrace(MemScope scope, void* ptr) {
		if (Opt<u64> idx = ptrToTrace.Find(ptr); idx.hasVal) {
			Trace* trace = &traces[idx.val];
			Assert(trace->allocs > 0);
			trace->allocs--;
			trace->bytes -= size;
			ptrToTrace.Remove(ptr);
		}
	}
};

MemTrace memTraceApi;

MemTraceApi* MemTraceApi::Get() {
	return &memTraceApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC