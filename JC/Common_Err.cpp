#include "JC/Common.h"

#include "JC/Config.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static Arena* errArena;

Err::Err(SrcLoc sl, s8 ns, s8 code, VArgs args) {
	Assert(args.len % 2 == 0);
	obj  = (ErrObj*)errArena->Alloc(sizeof(ErrObj) + (args.len > 0 ? (args.len / 2) - 1 : 0) * sizeof(ErrArg));
	obj->prev    = 0;
	obj->ns      = ns;
	obj->code    = code;
	obj->sl      = sl;
	obj->argsLen = args.len / 2;
	for (u32 i = 0; i < args.len / 2; i++) {
		Assert(args.args[i * 2].type == VArgType::S8);
		obj->args[i].name = s8(args.args[i * 2].s.data, args.args[i * 2].s.len);
		obj->args[i].arg  = args.args[i * 2 + 1];
	}

	#if defined DebugBreakOnErr
	if (Sys::IsDebuggerPresent()) {
		Sys_DebuggerBreak();
	}
	#endif	// DebugBreakOnErr
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC