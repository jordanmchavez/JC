#include "JC/Err.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] ErrArg {
	s8  name;
	Arg arg;
};

struct [[nodiscard]] Err {
	Err*    prev;
	SrcLoc  sl;
	i32     line;
	ErrCode ec;
	u32     argsLen;
	ErrArg  args[1];	// variable length
};

//--------------------------------------------------------------------------------------------------

Err* _VMakeErr(Err* prev, SrcLoc sl, ErrCode ec, const s8* argNames, const Arg* args, u32 argsLen) {
	Err* err     = (Err*)GetMemApi()->Temp()->Alloc(sizeof(Err) + (argsLen > 0 ? argsLen - 1 : 0) * sizeof(ErrArg));
	err->prev    = (Err*)prev;
	err->sl      = sl;
	err->ec      = ec;
	err->argsLen = argsLen;
	for (u32 i = 0; i < argsLen; i++) {
		err->args[i].name = argNames[i];
		err->args[i].arg  = args[i];
	}
	return err;
}

//--------------------------------------------------------------------------------------------------

void AddErrStr(Err* err, Array<char>* out) {
	for (Err* e = err; e; e = e->prev) {
		Fmt(out, "{}-{}:", e->ec.ns, e->ec.code);
	}
	out->data[out->len - 1] = '\n';	// overwrite trailing ':'
	for (Err* e = err; e; e = e->prev) {
		Fmt(out, "  {}({}): {}-{}\n", e->sl.file, e->sl.line, e->ec.ns, e->ec.code);
		for (u32 i = 0; i < e->argsLen; i++) {
			Fmt(out, "    '{}' = {}\n", e->args[i].name, e->args[i].arg);
		}
	}
	out->len--;	// remove trailing '\n'
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC