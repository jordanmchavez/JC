#include "JC/Err.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] ErrArg {
	s8  name = {};
	Arg arg  = {};
};

struct [[nodiscard]] Err {
	Err*    prev    = 0;
	s8      file    = 0;
	i32     line    = 0;
	ErrCode ec      = {};
	u32     argsLen = 0;
	ErrArg  args[1] = {};	// variable length
};

//--------------------------------------------------------------------------------------------------

Err* _VMakeErr(Mem* mem, Err* prev, s8 file, i32 line, ErrCode ec, const ErrArg* errArgs, u32 errArgsLen) {
	Err* err     = (Err*)mem->Alloc(sizeof(Err) + (errArgsLen > 0 ? errArgsLen - 1 : 0) * sizeof(ErrArg));
	err->prev    = (Err*)prev;
	err->file    = file;
	err->line    = line;
	err->ec      = ec;
	err->argsLen = errArgsLen;
	MemCpy(err->args, errArgs, errArgsLen * sizeof(errArgs[0]));
	return err;
}

//--------------------------------------------------------------------------------------------------

void AddErrStr(Err* err, Array<char>* out) {
	for (Err* e = err; e; e = e->prev) {
		Fmt(out, "{}-{}:", e->ec.ns, e->ec.code);
	}
	out->data[out->len - 1] = '\n';	// overwrite trailing ':'
	for (Err* e = err; e; e = e->prev) {
		Fmt(out, "  {}({}): {}-{}\n", e->file, e->line, e->ec.ns, e->ec.code);
		for (u32 i = 0; i < e->argsLen; i++) {
			Fmt(out, "    '{}' = {}\n", e->args[i].name, e->args[i].arg);
		}
	}
	out->len--;	// remove trailing '\n'
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC