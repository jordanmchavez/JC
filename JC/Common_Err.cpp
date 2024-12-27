#include "JC/Common.h"

#include "JC/Array.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct ErrObj {
};

Err* MakeErr(Arena* arena, SrcLoc sl, s8 ns, s8 code, Span<NamedVal> namedVals) {
	Err* err = arena->AllocT<Err>();
	err->arena = arena;
	err->prev  = 0;
	err->ns    = ns;
	err->code  = code;
	err->sl    = sl;
	err->namedVals = arena->AllocT<
	for (u32 i = 0; i < vargs.len / 2; i++) {
		Assert(vargs.vargs[i * 2].type == VArgType::S8);
		obj->namedArgs[i].name = s8(vargs.vargs[i * 2].s.data, vargs.vargs[i * 2].s.len);
		obj->namedArgs[i].varg = vargs.vargs[i * 2 + 1];
	}
	obj->namedArgsLen = vargs.len / 2;

	#if defined DebugBreakOnErr
	if (Sys::IsDebuggerPresent()) {
		Sys_DebuggerBreak();
	}
	#endif	// DebugBreakOnErr
}

void Err::AddNamedVals(Span<NamedVal> newNamedVals) {
	
	for (u32 i = 0; i < vargs.len / 2; i++) {
		Assert(vargs.vargs[i * 2].type == VArgType::S8);
		obj->namedArgs[obj->namedArgsLen + i].name = s8(vargs.vargs[i * 2].s.data, vargs.vargs[i * 2].s.len);
		obj->namedArgs[obj->namedArgsLen + i].varg = vargs.vargs[i * 2 + 1];
	}
	obj->namedArgsLen += vargs.len / 2;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC