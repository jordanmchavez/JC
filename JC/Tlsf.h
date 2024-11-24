#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Tlsf {
	static constexpr ErrCode Err_BitMapsMismatch    = { .ns = "tlsf", .code = 1 };
	static constexpr ErrCode Err_FreeBlocksMismatch = { .ns = "tlsf", .code = 2 };
	static constexpr ErrCode Err_NotMarkedFree      = { .ns = "tlsf", .code = 3 };
	static constexpr ErrCode Err_NotCoalesced       = { .ns = "tlsf", .code = 4 };
	static constexpr ErrCode Err_BlockTooSmall      = { .ns = "tlsf", .code = 5 };
	static constexpr ErrCode Err_BlockIndex         = { .ns = "tlsf", .code = 6 };

	u64 opaque = 0;
	
	void  Init(void* ptr, u64 size);
	void  AddChunk(void* ptr, u64 size);
	void* Alloc(u64 size);
	bool  Extend(void* ptr, u64 size);
	void  Free(void* ptr);
	Res<> CheckIntegrity();
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC