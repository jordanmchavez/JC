#include "JC/Log.h"
#include "JC/Common.h"

namespace JC::Log {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxFns  = 32;
static constexpr U32 MaxLine = 4096;

static Mem  permMem;
static Mem  tempMem;
static Fn** fns;
static U32  fnsLen;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMemIn, Mem tempMemIn) {
	permMem = permMemIn;
	tempMem = tempMemIn;
	fns = Mem::AllocT<Fn*>(permMem, MaxFns);
}

//--------------------------------------------------------------------------------------------------

void Printv(SrcLoc sl, Level level, char const* fmt, Span<Arg const> args) {
	PrintBuf pb(tempMem);
	pb.Printv(fmt, args);
	pb.Add('\n');
	pb.Add('\0');
	Msg msg = {
		.sl      = sl,
		.level   = level,
		.line    = pb.data,
		.lineLen = pb.len - 1,
	};
	for (U32 i = 0; i < fnsLen; i++) {
		(*fns[i])(&msg);
	}
}

//--------------------------------------------------------------------------------------------------

void PrintErr(SrcLoc sl, const Err* err) {
	Assert(err);

	PrintBuf pb(tempMem);
	for (const Err* e = err; e; e = e->prev) {
		pb.Printf("%s-", e->ns);
		if (e->sCode.len) {
			pb.Printf("%s:", e->sCode);
		} else {
			pb.Printf("%u:", e->uCode);
		}
	}
	pb.Remove(1);
	pb.Add('\n');
	for (const Err* e = err; e; e = e->prev) {
		pb.Printf("\t%s(%u): %s-", e->sl.file, e->sl.line, e->ns);
		if (e->sCode.len) {
			pb.Printf("%s\n", e->sCode);
		} else {
			pb.Printf("%u\n", e->uCode);
		}
		for (U32 i = 0; i < e->namedArgsLen; i++) {
			pb.Printf("\t\t%s = %a\n", e->namedArgs[i].name, e->namedArgs[i].arg);
		}
	}
	pb.Add('\n');
	pb.Add('\0');
	Msg msg = {
		.sl      = sl,
		.level   = Log::Level::Error,
		.line    = pb.data,
		.lineLen = pb.len - 1,
	};

	for (U32 i = 0; i < fnsLen; i++) {
		(*fns[i])(&msg);
	}
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

}	// namespace JC::Log