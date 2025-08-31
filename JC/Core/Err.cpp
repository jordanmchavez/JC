#include "JC/Core.h"	// not Core/Err.h to preserve core inclusion order

#include "JC/Array.h"
#include "JC/Config.h"
#include "JC/Fmt.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static Mem::TempAllocator* tempAllocator;

void Err::Init(Mem::TempAllocator* tempAllocatorIn) {
	tempAllocator = tempAllocatorIn;
}

//--------------------------------------------------------------------------------------------------

void Err::Init(Str ns, Str code, Span<const NamedArg> namedArgs, SrcLoc sl) {
	JC_ASSERT(namedArgs.len <= MaxArgs);

	data = tempAllocator->AllocT<Data>();
	data->prev = 0;
	data->sl   = sl;
	data->ns   = ns;
	data->code = code;
	for (U64 i = 0; i < namedArgs.len; i++) {
		data->namedArgs[i].name = namedArgs[i].name;
		data->namedArgs[i].arg  = namedArgs[i].arg;	// TODO: should this be a string copy here?
	}
	data->namedArgsLen = (U32)namedArgs.len;

	#if defined JC_DEBUG_BREAK_ON_ERR
	if (Sys::IsDebuggerPresent()) {
		JC_DEBUGGER_BREAK();
	}
	#endif	// JC_DEBUG_BREAK_ON_ERR
}

void Err::Init(Str ns, U64 code, Span<const NamedArg> namedArgs, SrcLoc sl) {
	Init(ns, Fmt::Printf(tempAllocator, "{}", code), namedArgs, sl);
}

//--------------------------------------------------------------------------------------------------

Err Err::Push(Err err) {
	err.data->prev = data;
	return err;
}

//--------------------------------------------------------------------------------------------------

Str Err::GetStr() {
	JC_ASSERT(data);
	Array<char> a(tempAllocator);
	Fmt::Printf(&a, "Error: ");
	for (Data* d = data; d; d = d->prev) {
		Fmt::Printf(&a, "{}-{} -> ", d->ns, d->code);
	}
	a.len -= 4;
	a.Add('\n');
	for (Data* d = data; d; d = d->prev) {
		Fmt::Printf(&a, "{}-{}:\n", d->ns, d->code);
		for (U32 i = 0; i < d->namedArgsLen; i++) {
			Fmt::Printf(&a, "  {}={}\n", d->namedArgs[i].name, d->namedArgs[i].arg);
		}
	}
	a.len--;
	return Str(a.data, a.len);
}

//--------------------------------------------------------------------------------------------------

// TODO: res/err tests

}	// namespace JC