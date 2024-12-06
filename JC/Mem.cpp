#include "JC/Mem.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Hash.h"
#include "JC/Map.h"
#include "JC/Sys.h"
#include "JC/Tlsf.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Scope : Mem {
	s8     name     = {};
	u64    bytes    = 0;
	u32    allocs   = 0;
	u32    children = 0;
	Scope* parent   = 0;	// doubles as next link in free list

	void* Alloc(u64 size, SrcLoc sl) override;
	bool  Extend(void* p, u64 oldSize, u64 newSize, SrcLoc sl) override;
	void  Free(void* p, u64 size) override;
};
//--------------------------------------------------------------------------------------------------

struct TempMemObj : TempMem {
	void* Alloc(u64 size, SrcLoc sl) override;
	bool  Extend(void* p, u64 oldSize, u64 newSize, SrcLoc sl) override;
	void  Free(void* p, u64 size) override;
};

//--------------------------------------------------------------------------------------------------

struct Trace {
	Scope* scope  = 0;
	SrcLoc sl     = {};
	u64    bytes  = 0;
	u32    allocs = 0;
};

//--------------------------------------------------------------------------------------------------

struct MemApiObj : MemApi {
	static constexpr u32 MaxScopes     = 1024;
	static constexpr u64 TempAlignSize = 8;
	static constexpr u64 TempReserve   = 4 * 1024 * 1024;
	static constexpr u64 TempMinCommit = 64 * 1024;

	MemLeakReporter* memLeakReporter   = 0;
	Tlsf             tlsf              = {};
	u64              permSize          = 0;
	Scope            scopes[MaxScopes] = {};
	Scope*           freeScopes        = 0;

	Array<Trace>     traces            = {};
	Map<u64, u64>    slToTrace         = {};
	Map<void*, u64>  ptrToTrace        = {};

	u8*              tempBegin         = 0;
	u8*              tempUsed          = 0;
	u8*              tempHigh          = 0;
	u8*              tempCommit        = 0;
	u8*              tempReserve       = 0;
	TempMemObj       tempMemObj        = {};

	//----------------------------------------------------------------------------------------------

	void Init() override {
		permSize = (u64)4 * 1024 * 1024;
		void* const permChunk = Sys::VirtualAlloc(permSize);
		tlsf.Init(permChunk, permSize);

		scopes[0].name = "Mem";
		scopes[1].name = "Perm";
		for (u32 i = 2; i < MaxScopes - 1; i++) {
			scopes[i].parent = &scopes[i + 1];
		}
		scopes[MaxScopes - 1].parent = 0;
		freeScopes = &scopes[2];

		traces.mem = &scopes[0];
		slToTrace.Init(&scopes[0]);
		ptrToTrace.Init(&scopes[0]);

		tempBegin     = (u8*)Sys::VirtualReserve(TempReserve);
		Sys::VirtualCommit(tempBegin, TempMinCommit);
		tempUsed      = tempBegin;
		tempCommit    = tempBegin + TempMinCommit;
		tempReserve   = tempBegin + TempReserve;
	}

	//----------------------------------------------------------------------------------------------

	void SetLeakReporter(MemLeakReporter* memLeakReporterIn) override {
		memLeakReporter = memLeakReporterIn;
	}

	//----------------------------------------------------------------------------------------------

	void Frame(u64 frame) override {
		if (tempUsed > tempHigh) {
			tempHigh = tempUsed;
		}

		if (!(frame & ((u64)1024 - 1))) {
			const u64 committed = (u64)(tempCommit - tempBegin);
			const u64 needed   = AlignPow2((u64)(tempCommit - tempHigh));
			Assert(needed <= committed);
			if (needed < committed && needed > TempMinCommit) {
				Sys::VirtualDecommit(tempBegin + needed, (u64)(committed - needed));
				tempCommit = tempBegin + needed;
			}
		}

		tempUsed = tempBegin;
	}

	//----------------------------------------------------------------------------------------------

	Mem*     Root() override { return &scopes[1]; }
	TempMem* Temp() override { return &tempMemObj; }

	//----------------------------------------------------------------------------------------------

	Mem* Create(s8 name, Mem* parent) override {
		Scope* const parentScope = (Scope*)parent;
		Assert(!parent || (parentScope >= &scopes[1] && parentScope<= &scopes[MaxScopes]));
		Assert(freeScopes);
		Scope* const scope = freeScopes;
		freeScopes = freeScopes->parent;
		scope->name     = name;
		scope->bytes    = 0;
		scope->allocs   = 0;
		scope->children = 0;
		scope->parent   = parentScope;
		if (scope->parent) {
			scope->parent->children++;
		}
		return scope;
	}

	//----------------------------------------------------------------------------------------------

	void Destroy(Mem* mem) override {
		Scope* scope = (Scope*)mem;
		Assert(scope >= scopes + 2 && scope < scopes + MaxScopes);

		if (memLeakReporter && (scope->bytes > 0 || scope->allocs > 0 || scope->children > 0)) {
			memLeakReporter->LeakedScope(scope->name, scope->bytes, scope->allocs, scope->children);
			if (scope->bytes > 0 || scope->allocs > 0) {
				for (u64 i = 0; i < traces.len; i++) {
					if (traces[i].scope == scope) {
						memLeakReporter->LeakedAlloc(traces[i].sl, traces[i].bytes, traces[i].allocs);
					}
				}
			}
			if (scope->children > 0) {
				for (u32 i = 0; i < MaxScopes; i++) {
					if (scopes[i].parent == scope) {
						memLeakReporter->LeakedChild(scopes[i].name, scopes[i].bytes, scopes[i].allocs);
					}
				}
			}
		}
		MemSet(scope, 0, sizeof(*scope));
		scope->parent = freeScopes;
		freeScopes = scope;
	}

	void AddTrace(Scope* scope, void* p, u64 size, SrcLoc sl) {
		if (scope == &scopes[0]) {
			return;
		}
		u64 key = HashCombine(Hash(sl.file), &sl.line, sizeof(sl.line));
		if (Opt<u64> idx = slToTrace.Find(key)) {
			Trace* trace = &traces[idx.val];
			trace->allocs++;
			trace->bytes += size;
			ptrToTrace.Put(p, idx.val);
		} else {
			traces.Add(Trace {
				.scope  = scope,
				.sl     = sl,
				.bytes  = size,
				.allocs = 1,
			});
			ptrToTrace.Put(p, traces.len - 1);
			slToTrace.Put(key, traces.len - 1);
		}
	}

	void RemoveTrace(Scope* scope, void* p, u64 size) {
		if (scope == &scopes[0]) {
			return;
		}
		if (Opt<u64> idx = ptrToTrace.Find(p); idx.hasVal) {
			Trace* trace = &traces[idx.val];
			Assert(trace->scope == scope);
			Assert(trace->allocs > 0);
			trace->bytes -= size;
			trace->allocs--;
			ptrToTrace.Remove(p);
		}
	}
};

//--------------------------------------------------------------------------------------------------

static MemApiObj memApi;

MemApi* GetMemApi() {
	return &memApi;
}

//--------------------------------------------------------------------------------------------------

void* Scope::Alloc(u64 size, SrcLoc sl) {
	if (size == 0) {
		return 0;
	}
	void* p = memApi.tlsf.Alloc(size);
	if (!p) {
		u64 chunkSize = memApi.permSize;
		while (chunkSize - 8 < size) {
			chunkSize *= 2;
		}
		void* const chunk = Sys::VirtualAlloc(chunkSize);
		memApi.permSize += chunkSize;

		memApi.tlsf.AddChunk(chunk, chunkSize);
		p = memApi.tlsf.Alloc(size);
		Assert(p);
	}
	memApi.AddTrace(this, p, size, sl);
	return p;
}

//--------------------------------------------------------------------------------------------------

bool Scope::Extend(void* p, u64 oldSize, u64 newSize, SrcLoc sl) {
	if (!p || !memApi.tlsf.Extend(p, newSize)) {
		return false;
	}
	memApi.RemoveTrace(this, p, oldSize);
	memApi.AddTrace(this, p, newSize, sl);
	return true;
}

//--------------------------------------------------------------------------------------------------

void Scope::Free(void* p, u64 size) {
	if (p) {
		memApi.tlsf.Free(p);
		memApi.RemoveTrace(this, p, size);
	}
}

//--------------------------------------------------------------------------------------------------

void* TempMemObj::Alloc(u64 size, SrcLoc) {
	size = AlignUp(size, memApi.TempAlignSize);
	Assert(memApi.tempCommit >= memApi.tempUsed);
	const u64 tempAvail = (u64)(memApi.tempCommit - memApi.tempUsed);
	if (tempAvail < size) {
		const u64 committed = (u64)(memApi.tempCommit - memApi.tempBegin);
		u64 nextCommitted = Max((u64)4096, committed * 2);
		while (tempAvail + nextCommitted < size) {
			nextCommitted *= 2;
			Assert(memApi.tempBegin + nextCommitted < memApi.tempReserve);
		}
		const u64 commitExtend = nextCommitted - committed;
		Sys::VirtualCommit(memApi.tempCommit, commitExtend);
		memApi.tempCommit += commitExtend;
	}
	void* p = memApi.tempUsed;
	memApi.tempUsed += size;
	Assert(memApi.tempUsed <= memApi.tempCommit);
	return p;
}

//--------------------------------------------------------------------------------------------------

bool TempMemObj::Extend(void* p, u64 oldSize, u64 newSize, SrcLoc) {
	if (!p) {
		return false;
	}

	oldSize = AlignUp(oldSize, memApi.TempAlignSize);
	if (p != memApi.tempUsed - oldSize) {
		return false;
	}

	newSize = AlignUp(newSize, memApi.TempAlignSize);
	memApi.tempUsed -= oldSize;
	Assert(memApi.tempCommit >= memApi.tempUsed);
	const u64 tempAvail = (u64)(memApi.tempCommit - memApi.tempUsed);
	if (tempAvail < newSize) {
		const u64 committed = (u64)(memApi.tempCommit - memApi.tempBegin);
		u64 nextCommitted = Max((u64)4096, committed * 2);
		while (tempAvail + nextCommitted < newSize) {
			nextCommitted *= 2;
			Assert(memApi.tempBegin + nextCommitted < memApi.tempReserve);
		}
		const u64 commitExtend = nextCommitted - committed;
		Sys::VirtualCommit(memApi.tempCommit, commitExtend);
		memApi.tempCommit += commitExtend;
	}
	memApi.tempUsed += newSize;
	return true;
}

//--------------------------------------------------------------------------------------------------

void TempMemObj::Free(void* p, u64 size) {
	if (p == memApi.tempUsed - size) {
		memApi.tempUsed -= size;
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC