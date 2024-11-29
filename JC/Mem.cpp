#include "JC/Mem.h"

#include "Bit.h"
#include "JC/MemTrace.h"
#include "JC/Sys.h"
#include "JC/Tlsf.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct MemObj : Mem {
	MemScope scope  = {};
	MemObj*  parent = 0;	// doubles as next link in free list

	void* Alloc(u64 size, SrcLoc sl) override;
	bool  Extend(void* ptr, u64 size, SrcLoc sl) override;
	void  Free(void* ptr) override;
};

//--------------------------------------------------------------------------------------------------

struct TempMemObj : TempMem {
	void* Alloc(u64 size, SrcLoc sl) override;
	bool  Extend(void* ptr, u64 size, SrcLoc sl) override;
	u64   Mark() override;
	void  Reset(u64 mark) override;
};

//--------------------------------------------------------------------------------------------------

struct Mem : MemApi {
	static constexpr u32 MaxMemObjs    = 1024;
	static constexpr u64 TempAlignSize = 8;

	MemTraceApi* memTraceApi         = 0;

	Tlsf         tlsf                = {};
	u64          permSize            = 0;
	MemObj       memObjs[MaxMemObjs] = {};
	MemObj*      freeMemObjs         = 0;

	u8*          tempBegin           = 0;
	u8*          tempUsed            = 0;
	u8*          tempHigh            = 0;
	u8*          tempCommit          = 0;
	u8*          tempReserve         = 0;
	u8*          tempLastAlloc       = 0;
	TempMemObj   tempMemObj          = {};

	//----------------------------------------------------------------------------------------------

	void Init(MemTraceApi* memTraceApiIn) override {
		memTraceApi = memTraceApiIn;

		permSize = 4 * 1024 * 1024;
		void* const permChunk = Sys::VirtualAlloc(permSize);
		memTraceApi->PermAlloc(permSize);
		tlsf.Init(permChunk, permSize);

		freeMemObjs = &memObjs[1];	// memObjs[0] is reserved for the perm allocator
		for (u32 i = 1; i < MaxMemObjs - 1; i++) {
			memObjs[i].parent = &memObjs[i + 1];
		}
		memObjs[MaxMemObjs - 1].parent = nullptr;

		constexpr u64 TempReserveSize = (u64)4 * 1024 * 1024 * 1024;
		tempBegin     = (u8*)Sys::VirtualReserve(TempReserveSize);
		tempUsed      = tempBegin;
		tempCommit    = tempBegin;
		tempReserve   = tempBegin + TempReserveSize;
		tempLastAlloc = 0;
	}

	//----------------------------------------------------------------------------------------------

	void Frame(u64 frame) override {
		if (tempUsed > tempHigh) {
			tempHigh = tempUsed;
		}

		if (!(frame & (1024 - 1))) {
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

	Mem*     Perm() override { return &memObjs[0]; }
	TempMem* Temp() override { return &tempMemObj; }

	//----------------------------------------------------------------------------------------------

	Mem* CreateScope(s8 name, Mem* parent) override {
		MemObj* const p = (MemObj*)parent;
		Assert(!parent || (p >= &memObjs[1] && p<= &memObjs[MaxMemObjs]));
		Assert(freeMemObjs);
		MemObj* const m = freeMemObjs;
		freeMemObjs = freeMemObjs->parent;
		m->scope  = memTraceApi->CreateScope(name, p ? p->scope : MemScope{});
		m->parent = p;
		return m;
	}

	//----------------------------------------------------------------------------------------------

	void DestroyScope(Mem* mem) override {
		MemObj* m = (MemObj*)mem;
		memTraceApi->DestroyScope(m->scope);
		MemSet(m, 0, sizeof(*m));
		m->parent = freeMemObjs;
		freeMemObjs = m;
	}
};

//--------------------------------------------------------------------------------------------------

static Mem memApi;

MemApi* MemApi::Get() {
	return &memApi;
}

//--------------------------------------------------------------------------------------------------

void* MemObj::Alloc(u64 size, SrcLoc sl) {
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
		memApi.memTraceApi->PermAlloc(chunkSize);
		memApi.permSize += chunkSize;

		memApi.tlsf.AddChunk(chunk, chunkSize);
		ptr = memApi.tlsf.Alloc(size);
		Assert(ptr);
	}
	memApi.memTraceApi->AddTrace(scope, ptr, size, sl);
	return ptr;
}

//--------------------------------------------------------------------------------------------------

bool MemObj::Extend(void* ptr, u64 size, SrcLoc sl) {
	if (ptr && memApi.tlsf.Extend(ptr, size)) {
		memApi.memTraceApi->RemoveTrace(scope, ptr);
		memApi.memTraceApi->AddTrace(scope, ptr, size, sl);
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------

void MemObj::Free(void* ptr) {
	if (ptr) {
		memApi.tlsf.Free(ptr);
		memApi.memTraceApi->RemoveTrace(scope, ptr);
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
	memApi.tempLastAlloc = memApi.tempUsed;
	void* p = memApi.tempUsed;
	memApi.tempUsed += size;
	return p;
}

//--------------------------------------------------------------------------------------------------

bool TempMemObj::Extend(void* ptr, u64 size, SrcLoc) {
	if (!ptr || ptr != memApi.tempLastAlloc) {
		return false;
	}

	size = AlignUp(size, memApi.TempAlignSize);
	memApi.tempUsed = memApi.tempLastAlloc;

	Assert(memApi.tempCommit >= memApi.tempUsed);
	const u64 tempAvail = (u64)(memApi.tempCommit - memApi.tempUsed);
	if (tempAvail < size) {
		u64 extend = Max((u64)4096, (u64)(memApi.tempCommit - memApi.tempReserve) * 2);
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