#include "JC/Err.h"

#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Mem.h"
#include "JC/Sys.h"

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

static Mem::TempAllocator* tempAllocator;

//--------------------------------------------------------------------------------------------------

Err* Make(SrcLoc sl, Err* prev, Str ns, Str code, Span<const NamedArg> namedArgs) {
	JC_ASSERT(namedArgs.len <= Err::MaxNamedArgs);

	Err* const err = tempAllocator->AllocT<Err>();
	err->prev = prev;
	err->sl   = sl;
	err->ns   = ns;
	err->code = code;
	for (U64 i = 0; i < namedArgs.len; i++) {
		err->namedArgs[i].name = namedArgs[i].name;
		err->namedArgs[i].arg  = namedArgs[i].arg;	// TODO: should this be a string copy here?
	}
	err->namedArgsLen = (U32)namedArgs.len;

	#if defined JC_DEBUG_BREAK_ON_ERR
	if (Sys::IsDebuggerPresent()) {
		JC_DEBUGGER_BREAK;
	}
	#endif	// JC_DEBUG_BREAK_ON_ERR

	return err;
}

Err* Make(Err* prev, SrcLoc sl, Str ns, U64 code, Span<const NamedArg> namedArgs) {
	return Make(sl, prev, ns, Fmt::Printf(tempAllocator, "{}", code), namedArgs);
}

//--------------------------------------------------------------------------------------------------

Str MakeStr(const Err* err) {
	JC_ASSERT(err);
	Array<char> a(tempAllocator);
	Fmt::Printf(&a, "Error: ");
	for (const Err* e = err; e; e = e->prev) {
		Fmt::Printf(&a, "{}-{} -> ", e->ns, e->code);
	}
	a.len -= 4;
	a.Add('\n');
	for (const Err* e = err; e; e = e->prev) {
		Fmt::Printf(&a, "{}-{}:\n", e->ns, e->code);
		for (U32 i = 0; i < e->namedArgsLen; i++) {
			Fmt::Printf(&a, "  {}={}\n", e->namedArgs[i].name, e->namedArgs[i].arg);
		}
	}
	a.len--;
	return Str(a.data, a.len);
}

//--------------------------------------------------------------------------------------------------

// TODO: res/err tests

}	// namespace JC