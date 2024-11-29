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
	bool  Extend(void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) override;
	void  Free(void* ptr, u64 size) override;
};
//--------------------------------------------------------------------------------------------------

struct TempMemObj : TempMem {
	void* Alloc(u64 size, SrcLoc sl) override;
	bool  Extend(void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) override;
	u64   Mark() override;
	void  Reset(u64 mark) override;
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
	static constexpr u32 MaxScopes = 1024;
	static constexpr u64 TempAlignSize = 8;

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

		constexpr u64 TempReserveSize = (u64)4 * 1024 * 1024 * 1024;
		tempBegin     = (u8*)Sys::VirtualReserve(TempReserveSize);
		tempUsed      = tempBegin;
		tempCommit    = tempBegin;
		tempReserve   = tempBegin + TempReserveSize;
	}
	//----------------------------------------------------------------------------------------------

	void SetLeakReporter(MemLeakReporter* memLeakReporterIn) override {
		memLeakReporter = memLeakReporterIn;
	}

	void Frame(u64 frame) override {
		if (tempUsed > tempHigh) {
			tempHigh = tempUsed;
		}

		if (!(frame & ((u64)1024 - 1))) {
			u8* const tempCommitNeeded = (u8*)AlignPow2((u64)tempHigh);
			Assert(tempCommitNeeded <= tempCommit);
			if (tempCommitNeeded < tempCommit) {
				Sys::VirtualDecommit(tempCommitNeeded, (u64)(tempCommit - tempCommitNeeded));
			}
			tempCommit = tempCommitNeeded;
		}

		tempUsed = 0;
	}

	//----------------------------------------------------------------------------------------------

	Mem*     Perm() override { return &scopes[1]; }
	TempMem* Temp() override { return &tempMemObj; }

	//----------------------------------------------------------------------------------------------

	Mem* CreateScope(s8 name, Mem* parent) override {
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

	void DestroyScope(Mem* mem) override {
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

	void AddTrace(Scope* scope, void* ptr, u64 size, SrcLoc sl) {
		if (scope == &scopes[0]) {
			return;
		}
		u64 key = HashCombine(Hash(sl.file), &sl.line, sizeof(sl.line));
		if (Opt<u64> idx = slToTrace.Find(key)) {
			Trace* trace = &traces[idx.val];
			trace->allocs++;
			trace->bytes += size;
			ptrToTrace.Put(ptr, idx.val);
		} else {
			traces.Add(Trace {
				.scope  = scope,
				.sl     = sl,
				.bytes  = size,
				.allocs = 1,
			});
			ptrToTrace.Put(ptr, traces.len - 1);
			slToTrace.Put(key, traces.len - 1);
		}
	}

	void RemoveTrace(Scope* scope, void* ptr, u64 size) {
		if (scope == &scopes[0]) {
			return;
		}
		if (Opt<u64> idx = ptrToTrace.Find(ptr); idx.hasVal) {
			Trace* trace = &traces[idx.val];
			Assert(trace->scope == scope);
			Assert(trace->allocs > 0);
			trace->bytes -= size;
			trace->allocs--;
			ptrToTrace.Remove(ptr);
		}
	}
};

//--------------------------------------------------------------------------------------------------

static MemApiObj memApi;

MemApi* MemApi::Get() {
	return &memApi;
}

//--------------------------------------------------------------------------------------------------

void* Scope::Alloc(u64 size, SrcLoc sl) {
	if (size == 0) {
		return 0;
	}
	void* ptr = memApi.tlsf.Alloc(size);
	if (!ptr) {
		u64 chunkSize = memApi.permSize;
		while (chunkSize - 8 < size) {
			chunkSize *= 2;
		}
		void* const chunk = Sys::VirtualAlloc(chunkSize);
		memApi.permSize += chunkSize;

		memApi.tlsf.AddChunk(chunk, chunkSize);
		ptr = memApi.tlsf.Alloc(size);
		Assert(ptr);
	}
	memApi.AddTrace(this, ptr, size, sl);
	return ptr;
}

//--------------------------------------------------------------------------------------------------

bool Scope::Extend(void* ptr, u64 oldSize, u64 newSize, SrcLoc sl) {
	if (!ptr || !memApi.tlsf.Extend(ptr, newSize)) {
		return false;
	}
	memApi.RemoveTrace(this, ptr, oldSize);
	memApi.AddTrace(this, ptr, newSize, sl);
	return true;
}

//--------------------------------------------------------------------------------------------------

void Scope::Free(void* ptr, u64 size) {
	if (ptr) {
		memApi.tlsf.Free(ptr);
		memApi.RemoveTrace(this, ptr, size);
	}
}

//--------------------------------------------------------------------------------------------------

void* TempMemObj::Alloc(u64 size, SrcLoc) {
	size = AlignUp(size, memApi.TempAlignSize);
	Assert(memApi.tempCommit >= memApi.tempUsed);
	const u64 tempAvail = (u64)(memApi.tempCommit - memApi.tempUsed);
	if (tempAvail < size) {
		u64 extend = Max((u64)4096, (u64)(memApi.tempCommit - memApi.tempBegin) * 2);
		while (tempAvail + extend < size) {
			extend *= 2;
			Assert(memApi.tempCommit + extend < memApi.tempReserve);
		}
		Sys::VirtualCommit(memApi.tempCommit, extend);
		memApi.tempCommit += extend;
	}
	void* p = memApi.tempUsed;
	memApi.tempUsed += size;
	return p;
}

//--------------------------------------------------------------------------------------------------

bool TempMemObj::Extend(void* ptr, u64 oldSize, u64 newSize, SrcLoc) {
	if (!ptr) {
		return false;
	}

	oldSize = AlignUp(oldSize, memApi.TempAlignSize);
	if (ptr != memApi.tempUsed - oldSize) {
		return false;
	}

	newSize = AlignUp(newSize, memApi.TempAlignSize);
	memApi.tempUsed -= oldSize;
	Assert(memApi.tempCommit >= memApi.tempUsed);
	const u64 tempAvail = (u64)(memApi.tempCommit - memApi.tempUsed);
	if (tempAvail < newSize) {
		u64 extend = Max((u64)4096, (u64)(memApi.tempCommit - memApi.tempReserve) * 2);
		while (tempAvail + extend < newSize) {
			extend *= 2;
			Assert(memApi.tempCommit + extend < memApi.tempReserve);
		}
		Sys::VirtualCommit(memApi.tempCommit, extend);
		memApi.tempCommit += extend;
	}
	memApi.tempUsed += newSize;
	return true;
}

//--------------------------------------------------------------------------------------------------

u64 TempMemObj::Mark() {
	Assert(memApi.tempUsed >= memApi.tempBegin);
	return memApi.tempUsed - memApi.tempBegin;
}

//--------------------------------------------------------------------------------------------------

void TempMemObj::Reset(u64 mark) {
	Assert(memApi.tempUsed >= memApi.tempBegin);
	Assert(mark <= (u64)(memApi.tempUsed - memApi.tempBegin));
	memApi.tempUsed = memApi.tempBegin + mark;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC