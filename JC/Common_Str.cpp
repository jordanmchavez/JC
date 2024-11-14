#include "JC/Common.h"

#include "JC/Allocator.h"
#include "JC/Map.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u64 ReserveSize = (u64)4 * 1024 * 1024;

static Allocator*            allocator        = nullptr;
static VirtualMemoryApi*     virtualMemoryApi = nullptr;
static char*                 mem              = nullptr;
static u64                   used             = 0;
static u64                   committed        = 0;
static Map<u64, const char*> hashToPtr;
static Map<const char*, u64> ptrToLen;

//--------------------------------------------------------------------------------------------------

u64 Str::Len() const {
	return *data ? ptrToLen.Find(data).Or(0) : 0;
}

//--------------------------------------------------------------------------------------------------

void Str::Init(Allocator* allocatorIn, VirtualMemoryApi* virtualMemoryApiIn) {
	allocator        = allocatorIn;
	virtualMemoryApi = virtualMemoryApiIn;
	mem              = (char*)virtualMemoryApi->Reserve(ReserveSize);
	used             = 0;
	committed        = 0;
	hashToPtr.Init(allocator);
	ptrToLen.Init(allocator);
}

//--------------------------------------------------------------------------------------------------

constexpr u64 AlignUp(u64 u, u64 align) {
	return (u + (align - 1)) & ~(align - 1);
}

Str Str::Make(s8 str) {
	const u64 h = Hash(str);
	if (Opt<const char*> ptr = hashToPtr.Find(h); ptr.hasVal) {
		return Str { .data = ptr.val };
	}
	if (used + str.len + 1 > committed) {
		const u64 inc = Max(committed, (u64)4096);
		JC_ASSERT(committed + inc <= ReserveSize);
		virtualMemoryApi->Commit(mem + committed, inc);
		committed += inc;
	}
	char* ptr = (char*)(mem + used);
	memcpy(ptr, str.data, str.len);
	ptr[str.len] = '\0';
	used = AlignUp(used + str.len + 1, 8);
	hashToPtr.Put(h, ptr);
	ptrToLen.Put(ptr, str.len);
	return Str { .data = ptr };
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC