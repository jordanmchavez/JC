#include "JC/Core.h"	// not Core/Err.h to preserve core inclusion order

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

static Mem::TempAllocator* tempAllocator;

void Init(Mem::TempAllocator* tempAllocatorIn) {
	tempAllocator = tempAllocatorIn;
}

void Error::Init(Str ns, Str code, Span<NamedArg> namedArgs, SrcLoc sl) {
	Assert(namedArgs.len <= MaxArgs);

	data = tempAllocator->AllocT<Data>();
	data->prev  = 0;
	data->sl    = sl;
	data->ns    = ns;
	data->code  = code;
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

Error Error::Push(Error error) {
	error.data->prev = data;
	return error;
}

//--------------------------------------------------------------------------------------------------

// TODO: res/err tests

}	// namespace JC::Err