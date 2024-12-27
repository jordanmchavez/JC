#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static Arena* errArena;

void Err::SetArena(Arena* arena) {
	errArena = arena;
}

static constexpr u32 MaxNamedArgs = 32;

struct ErrObj {
	Err      prev                    = {};
	SrcLoc   sl                      = {};
	s8       ns                      = {};
	s8       s8Code                  = {};
	i64      i64Code                 = {};
	NamedArg namedArgs[MaxNamedArgs] = {};
	u32      namedArgsLen            = 0;
};
void Err::Init(SrcLoc sl, s8 ns, s8 s8Code, i64 i64Code, Span<NamedArg> namedArgs) {
	Assert(namedArgs.len <= MaxNamedArgs);

	ErrObj* const obj = errArena->AllocT<ErrObj>();
	obj->prev    = {};
	obj->sl      = sl;
	obj->ns      = ns;
	obj->s8Code  = s8Code;
	obj->i64Code = i64Code;
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
	((ErrObj*)err.handle)->prev.handle = handle;
	return err;
}

void Err::AddInternal(Span<NamedArg> namedArgs) {
	ErrObj* const obj = (ErrObj*)handle;
	Assert(obj->namedArgsLen + namedArgs.len < MaxNamedArgs);
	MemCpy(obj->namedArgs + obj->namedArgsLen, namedArgs.data, namedArgs.len * sizeof(NamedArg));	// TODO: should we copy the string arg?
	obj->namedArgsLen += (u32)namedArgs.len;
}

Err            Err::GetPrev()      const { return ((ErrObj*)handle)->prev;      }
SrcLoc         Err::GetSrcLoc()    const { return ((ErrObj*)handle)->sl;        }
s8             Err::GetNs()        const { return ((ErrObj*)handle)->ns;        }
s8             Err::GetS8Code()    const { return ((ErrObj*)handle)->s8Code;    }
i64            Err::GetI64Code()   const { return ((ErrObj*)handle)->i64Code;   }
Span<NamedArg> Err::GetNamedArgs() const { return ((ErrObj*)handle)->namedArgs; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC