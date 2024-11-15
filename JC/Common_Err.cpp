#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Panic.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static PanicApi* panicApi = nullptr;

// Implemented here rather than Panic.cpp
PanicApi* SetPanicApi(PanicApi* newPanicApi) {
	PanicApi* oldPanicApi = panicApi;
	panicApi = newPanicApi;
	return oldPanicApi;
}

void VPanic(s8 file, i32 line, s8 expr, s8 fmt, Args args) {
	static bool recursive = false;
	if (recursive) {
		Sys::Abort();
	}
	recursive = true;

	if (panicApi) {
		panicApi->VPanic(file, line, expr, fmt, args);
	} else {
		if (Sys::IsDebuggerPresent()) {
			JC_DEBUGGER_BREAK;
		}
		Sys::Abort();
	}
}

//--------------------------------------------------------------------------------------------------

Err* Err::VMake(TempAllocator* ta, Err* prev, s8 file, i32 line, ErrCode ec, const ErrArg* errArgs, u32 errArgsLen) {
	Err* err  = (Err*)ta->Alloc(sizeof(Err) + (errArgsLen > 0 ? errArgsLen - 1 : 0) * sizeof(ErrArg));
	err->prev    = (Err*)prev;
	err->file    = file;
	err->line    = line;
	err->ec      = ec;
	err->argsLen = errArgsLen;
	JC_MEMCPY(err->args, errArgs, errArgsLen * sizeof(errArgs[0]));
	return err;
}

//--------------------------------------------------------------------------------------------------

void Err::Str(Array<char>* arr) {
	for (Err* e = this; e; e = e->prev) {
		Fmt(arr, "{}-{}:", e->ec.ns, e->ec.code);
	}
	(*arr)[arr->len - 1] = '\n';	// trailing ':'
	for (Err* e = this; e; e = e->prev) {
		Fmt(arr, "  {}({}): {}-{}\n", e->file, e->line, e->ec.ns, e->ec.code);
		for (u32 i = 0; i < e->argsLen; i++) {
			Fmt(arr, "    '{}' = {}\n", e->args[i].name, e->args[i].arg);
		}
	}
	arr->len--;	// trailing '\n'
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC