#include "JC/Common.h"

#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

Err* VMakeErr(Err* prev, SrcLoc sl, ErrCode ec, Args args) {
	Assert(args.len % 2 == 0);
	Err* err     = (Err*)GetMemApi()->Temp()->Alloc(sizeof(Err) + (args.len > 0 ? (args.len / 2) - 1 : 0) * sizeof(ErrArg));
	err->prev    = prev;
	err->sl      = sl;
	err->ec      = ec;
	err->argsLen = args.len / 2;
	for (u32 i = 0; i < args.len / 2; i++) {
		Assert(args.args[i * 2].type == ArgType::S8);
		err->args[i].name = s8(args.args[i * 2].s.data, args.args[i * 2].s.len);
		err->args[i].arg  = args.args[i * 2 + 1];
	}
	return err;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC