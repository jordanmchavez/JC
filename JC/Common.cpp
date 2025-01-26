#include "JC/Common.h"

#include "JC/Config.h"
#include "JC/Mem.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

Str Copy(Str s, Mem::Allocator* allocator, SrcLoc sl) {
	return Str((const char*)allocator->Alloc(s.len, sl), s.len);
}

//--------------------------------------------------------------------------------------------------

void Err::Init(SrcLoc sl, Str ns, Str code, Span<NamedArg> namedArgs) {
	Assert(namedArgs.len <= MaxNamedArgs);

	data = Mem::TempAllocT<Data>();
	data->prev  = 0;
	data->sl    = sl;
	data->ns    = ns;
	data->code  = code;
	for (u64 i = 0; i < namedArgs.len; i++) {
		data->namedArgs[i].name = namedArgs[i].name;
		data->namedArgs[i].varg = namedArgs[i].varg;	// TODO: should this be a string copy here?
	}
	data->namedArgsLen = (u32)namedArgs.len;

	#if defined DebugBreakOnErr
	if (Sys::IsDebuggerPresent()) {
		Sys_DebuggerBreak();
	}
	#endif	// DebugBreakOnErr
}

Err Err::Push(Err err) {
	err.data->prev = data;
	return err;
}

void Err::AddInternal(Span<NamedArg> namedArgs) {
	Assert(data->namedArgsLen + namedArgs.len < MaxNamedArgs);
	memcpy(data->namedArgs + data->namedArgsLen, namedArgs.data, namedArgs.len * sizeof(NamedArg));	// TODO: should we copy the string arg?
	data->namedArgsLen += (u32)namedArgs.len;
}

//--------------------------------------------------------------------------------------------------

void* Mem::Allocator::Realloc(void* oldPtr, u64 oldSize, u64 newSize, SrcLoc sl) {
	if (!Extend(oldPtr, oldSize, newSize, sl)) {
		void* newPtr = Alloc(newSize, sl);
		memcpy(newPtr, oldPtr, oldSize);
		Free(oldPtr, oldSize);
		oldPtr = newPtr;
	}
	return oldPtr;
}

//--------------------------------------------------------------------------------------------------

// TODO: res/err tests

}	// namespace JC