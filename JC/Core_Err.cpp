#include "JC/Core.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Sys.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static TempAllocator* tempAllocator;

//--------------------------------------------------------------------------------------------------

void Err::Init(Err prev, SrcLoc sl, Str ns, Str code, Span<const NamedArg> namedArgs) {
	JC_ASSERT(namedArgs.len <= MaxNamedArgs);

	data = tempAllocator->AllocT<Data>();
	data->prev = prev.data;
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

void Err::Init(Err prev, SrcLoc sl, Str ns, U64 code, Span<const NamedArg> namedArgs) {
	Init(prev, sl, ns, Fmt::Printf(tempAllocator, "{}", code), namedArgs);
}

//--------------------------------------------------------------------------------------------------

Str Err::GetStr() const {
	Array<char> a(tempAllocator);
	Fmt::Printf(&a, "Error: ");
	for (const Data* d = data; d; d = d->prev) {
		Fmt::Printf(&a, "{}-{} -> ", d->ns, d->code);
	}
	a.len -= 4;
	a.Add('\n');
	for (const Data* d = data; d; d = d->prev) {
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