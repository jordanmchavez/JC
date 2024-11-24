#include "JC/Mem.h"

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

struct MemApiObj : MemApi {
	static constexpr u32 MaxMemObjs    = 1024;
	static constexpr u64 TempAlignSize = 8;

	MemTraceApi* memTraceApi         = 0;

	Tlsf         tlsf                = {};
	u64          permSize            = 0;
	MemObj       memObjs[MaxMemObjs] = {};
	MemObj*      freeMemObjs         = 0;

	u8*          tempBegin           = 0;
	u8*          tempUsed            = 0;
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

	Mem*     Perm() override { return &memObjs[0]; }
	TempMem* Temp() override { return &tempMemObj; }

	//----------------------------------------------------------------------------------------------

	void* Alloc(MemObj* mem, u64 size, SrcLoc sl) {
		if (size == 0) {
			return 0;
		}
		void* ptr = tlsf.Alloc(size);
		if (!ptr) {
			u64 chunkSize = permSize;
			while (chunkSize - 8 < size) {
				chunkSize *= 2;
			}
			void* const chunk = Sys::VirtualAlloc(chunkSize);
			memTraceApi->PermAlloc(chunkSize);
			permSize += chunkSize;

			tlsf.AddChunk(chunk, chunkSize);
			ptr = tlsf.Alloc(size);
			Assert(ptr);
		}
		memTraceApi->AddTrace(mem->scope, ptr, size, sl);
		return ptr;
	}

	//----------------------------------------------------------------------------------------------

	bool Extend(MemObj* mem, void* ptr, u64 size, SrcLoc sl) {
		if (ptr && tlsf.Extend(ptr, size)) {
			memTraceApi->RemoveTrace(mem->scope, ptr);
			memTraceApi->AddTrace(mem->scope, ptr, size, sl);
			return true;
		}
		return false;
	}

	//----------------------------------------------------------------------------------------------

	void Free(MemObj* mem, void* ptr) {
		if (ptr) {
			tlsf.Free(ptr);
			memTraceApi->RemoveTrace(mem->scope, ptr);
		}
	}

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

	//----------------------------------------------------------------------------------------------

	void* TempAlloc(u64 size) {
		size = AlignUp(size, TempAlignSize);
		Assert(tempCommit >= tempUsed);
		const u64 tempAvail = (u64)(tempCommit - tempUsed);
		if (tempAvail < size) {
			u64 extend = Max((u64)4096, (u64)(tempCommit - tempBegin) * 2);
			while (tempAvail + extend < size) {
				extend *= 2;
				Assert(tempCommit + extend < tempReserve);
			}
			Sys::VirtualCommit(tempCommit, extend);
			tempCommit += extend;
		}
		tempLastAlloc = tempUsed;
		void* p = tempUsed;
		tempUsed += size;
		return p;
	}

	//----------------------------------------------------------------------------------------------

	bool TempExtend(void* ptr, u64 size) {
		if (!ptr || ptr != tempLastAlloc) {
			return false;
		}

		size = AlignUp(size, TempAlignSize);
		tempUsed = tempLastAlloc;

		Assert(tempCommit >= tempUsed);
		const u64 tempAvail = (u64)(tempCommit - tempUsed);
		if (tempAvail < size) {
			u64 extend = Max((u64)4096, (u64)(tempCommit - tempReserve) * 2);
			while (tempAvail + extend < size) {
				extend *= 2;
				Assert(tempCommit + extend < tempReserve);
			}
			Sys::VirtualCommit(tempCommit, extend);
			tempCommit += extend;
		}
		void* p = tempUsed;
		tempUsed += size;
		return p;
	}

	//----------------------------------------------------------------------------------------------

	u64 TempMark() {
		Assert(tempUsed >= tempBegin);
		return tempUsed - tempBegin;
	}

	//----------------------------------------------------------------------------------------------

	void TempReset(u64 mark) {
		Assert(tempUsed >= tempBegin);
		Assert(mark <= (u64)(tempUsed - tempBegin));
		tempUsed = tempBegin + mark;
	}
};

//--------------------------------------------------------------------------------------------------

static MemApiObj memApiObj;

MemApi* MemApi::Get() {
	return &memApiObj;
}

//--------------------------------------------------------------------------------------------------

void* MemObj::Alloc(u64 size, SrcLoc sl)              { return memApiObj.Alloc(this, size, sl); }
bool  MemObj::Extend(void* ptr, u64 size, SrcLoc sl)  { return memApiObj.Extend(this, ptr, size, sl); }
void  MemObj::Free(void* ptr)                         { memApiObj.Free(this, ptr); }

void* TempMemObj::Alloc(u64 size, SrcLoc)             { return memApiObj.TempAlloc(size); }
bool  TempMemObj::Extend(void* ptr, u64 size, SrcLoc) { return memApiObj.TempExtend(ptr, size); }
u64   TempMemObj::Mark()                              { return memApiObj.TempMark(); }
void  TempMemObj::Reset(u64 mark)                     { memApiObj.TempReset(mark); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC