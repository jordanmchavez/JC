#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static Arena* errArena;

void Err::SetArena(Arena* arena) {
	errArena = arena;
}

static constexpr u32 MaxNamedArgs = 32;

struct ErrObj {
	ErrObj*  prev                    = 0;
	SrcLoc   sl                      = {};
	NamedArg namedArgs[MaxNamedArgs] = {};
	u32      namedArgsLen            = 0;
};
void Err::Init(SrcLoc sl, Span<NamedArg> namedArgs) {
	Assert(namedArgs.len <= MaxNamedArgs);

	ErrObj* const obj = errArena->AllocT<ErrObj>();
	obj->prev  = 0;
	obj->sl    = sl;
	for (u64 i = 0; i < namedArgs.len; i++) {
		obj->namedArgs[i].name = namedArgs[i].name;
		obj->namedArgs[i].varg = namedArgs[i].varg;	// TODO: should this be a string copy here?
	}
	obj->namedArgsLen = (u32)namedArgs.len;

	#if defined DebugBreakOnErr
	if (Sys::IsDebuggerPresent()) {
		Sys_DebuggerBreak();
	}
	#endif	// DebugBreakOnErr
}

Err Err::Push(Err err) {
	ErrObj* const obj = (ErrObj*)err.handle;
	obj->prev = (ErrObj*)handle;
	return err;
}

void Err::AddInternal(Span<NamedArg> namedArgs) {
	ErrObj* const obj = (ErrObj*)handle;
	Assert(obj->namedArgsLen + namedArgs.len < MaxNamedArgs);
	MemCpy(obj->namedArgs + obj->namedArgsLen, namedArgs.data, namedArgs.len * sizeof(NamedArg));	// TODO: should we copy the string arg?
	obj->namedArgsLen += (u32)namedArgs.len;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC