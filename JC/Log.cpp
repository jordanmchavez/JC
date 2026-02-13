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
	pb.Printf("%s%s(%u): ", level == Level::Error ? "!!! " : "", sl.file, sl.line);
	pb.Printv(fmt, args);
	pb.Add('\n');
	pb.Add('\0');
	Msg msg = {
		.line    = pb.data,
		.lineLen = pb.len - 1,
		.sl      = sl,
		.level   = level,
		.fmt     = fmt,
		.args    = args,
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