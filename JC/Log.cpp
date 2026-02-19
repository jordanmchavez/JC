#include "JC/Log.h"
#include "JC/Common.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxFns  = 32;

static Mem    tempMem;
static Fn*    fns[MaxFns];
static U32    fnsLen;

//--------------------------------------------------------------------------------------------------

void Init(Mem tempMemIn) {
	tempMem = tempMemIn;
	fnsLen = 0;
}

//--------------------------------------------------------------------------------------------------

void AddFn(Fn* fn) {
	Assert(fnsLen < MaxFns);
	fns[fnsLen++] = fn;
}

//--------------------------------------------------------------------------------------------------

void RemoveFn(Fn* fn) {
	for (U32 i = 0; i < fnsLen; i++) {
		if (fns[i] == fn) {
			fns[i] = fns[--fnsLen];
		}
	}
}

//--------------------------------------------------------------------------------------------------

void Printv(SrcLoc sl, Level level, char const* fmt, Span<Arg const> args) {
	MemScope memScope(tempMem);
	StrBuf sb(tempMem);
	sb.Printv(fmt, args);
	sb.Add('\n');
	sb.Add('\0');
	Msg msg = {
		.sl      = sl,
		.level   = level,
		.line    = sb.data,
		.lineLen = sb.len - 1,
	};
	for (U32 i = 0; i < fnsLen; i++) {
		(*fns[i])(&msg);
	}
}

//--------------------------------------------------------------------------------------------------

void PrintErr(SrcLoc sl, const Err* err) {
	Assert(err);
	MemScope memScope(tempMem);
	StrBuf sb(tempMem);
	for (const Err* e = err; e; e = e->prev) {
		sb.Printf("%s-", e->ns);
		if (e->sCode.len) {
			sb.Printf("%s:", e->sCode);
		} else {
			sb.Printf("%u:", e->uCode);
		}
	}
	sb.Remove(1);
	sb.Add('\n');
	for (const Err* e = err; e; e = e->prev) {
		sb.Printf("\t%s(%u): %s-", e->sl.file, e->sl.line, e->ns);
		if (e->sCode.len) {
			sb.Printf("%s\n", e->sCode);
		} else {
			sb.Printf("%u\n", e->uCode);
		}
		for (U32 i = 0; i < e->namedArgsLen; i++) {
			sb.Printf("\t\t%s = %a\n", e->namedArgs[i].name, e->namedArgs[i].arg);
		}
	}
	sb.Add('\n');
	sb.Add('\0');
	Msg msg = {
		.sl      = sl,
		.level   = Log::Level::Error,
		.line    = sb.data,
		.lineLen = sb.len - 1,
	};

	for (U32 i = 0; i < fnsLen; i++) {
		(*fns[i])(&msg);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Log