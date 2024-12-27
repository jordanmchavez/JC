#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------
struct ErrObj {
	static constexpr u32 MaxNamedVals = 16;

	Err*     prev                    = 0;
	SrcLoc   sl                      = {};
	s8       ns                      = {};
	s8       code                    = {};
	NamedVal NamedVals[MaxNamedVals] = {};
	u32      namedValsLen            = 0;
};


Err* Err::MakeInternal(Arena* arena, SrcLoc sl, s8 ns, s8 code, Span<NamedVal> namedVals) {
	Assert(namedVals.len <= MaxNamedVals);

	Err* err = arena->AllocT<Err>();
	err->arena = arena;
	err->prev  = 0;
	err->ns    = ns;
	err->code  = code;
	err->sl    = sl;
	for (u32 i = 0; i < namedVals.len; i++) {
		err->namedVals[i].name = namedVals[i].name;
		err->namedVals[i].val  = namedVals[i].val;	// TODO: should this be a string copy here?
	}
	err->namedValsLen = namedVals.len;

	#if defined DebugBreakOnErr
	if (Sys::IsDebuggerPresent()) {
		Sys_DebuggerBreak();
	}
	#endif	// DebugBreakOnErr

	return err;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC