#include "JC/Common.h"

#include "JC/Config.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u32 MaxErrs = 64;

static ErrObj errObjs[MaxErrs];
static u32    errObjsLen;

Err::Err(SrcLoc sl, s8 ns, s8 code, VArgs vargs) {
	Assert(errObjsLen < MaxErrs);
	Assert(vargs.len < MaxErrNamedArgs);
	Assert(vargs.len % 2 == 0);
	obj = &errObjs[errObjsLen++];
	obj->prev    = 0;
	obj->ns      = ns;
	obj->code    = code;
	obj->sl      = sl;

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

void Err::AddVArgs(VArgs vargs) {
	Assert(obj);
	Assert(obj->namedArgsLen + vargs.len < MaxErrNamedArgs);
	Assert(vargs.len % 2 == 0);
	for (u32 i = 0; i < vargs.len / 2; i++) {
		Assert(vargs.vargs[i * 2].type == VArgType::S8);
		obj->namedArgs[obj->namedArgsLen + i].name = s8(vargs.vargs[i * 2].s.data, vargs.vargs[i * 2].s.len);
		obj->namedArgs[obj->namedArgsLen + i].varg = vargs.vargs[i * 2 + 1];
	}
	obj->namedArgsLen += vargs.len / 2;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC