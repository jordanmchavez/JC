#include "JC/Core.h"	// not Core/Err.h to preserve core inclusion order

#include "JC/Fmt.h"	// not Core/Err.h to preserve core inclusion order

namespace JC {

//--------------------------------------------------------------------------------------------------

static Mem::TempAllocator* tempAllocator;

void Err::SetTempAllocator(Mem::TempAllocator* tempAllocatorIn) {
	tempAllocator = tempAllocatorIn;
}

void Err::Init(Str ns, Str code, Span<NamedArg> namedArgs, SrcLoc sl) {
	Assert(namedArgs.len <= MaxArgs);

	data = tempAllocator->AllocT<Data>();
	data->prev = 0;
	data->sl   = sl;
	data->ns   = ns;
	data->code = code;
	for (u64 i = 0; i < namedArgs.len; i++) {
		data->namedArgs[i].name = namedArgs[i].name;
		data->namedArgs[i].arg  = namedArgs[i].arg;	// TODO: should this be a string copy here?
	}
	data->namedArgsLen = (u32)namedArgs.len;

	#if defined DebugBreakOnErr
	if (Sys::IsDebuggerPresent()) {
		Sys_DebuggerBreak();
	}
	#endif	// DebugBreakOnErr
}

void Err::Init(Str ns, i64 code, Span<NamedArg> namedArgs, SrcLoc sl) {
	Init(ns, Fmt::Printf(tempAllocator, "{}", code), namedArgs, sl);
}

Err Err::Push(Err err) {
	err.data->prev = data;
	return err;
}

//--------------------------------------------------------------------------------------------------

// TODO: res/err tests

}	// namespace JC