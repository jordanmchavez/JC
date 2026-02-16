#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxErrs   = 256;
static constexpr U32 MaxStrBuf = 64 * 1024;

static Err  errs[MaxErrs];
static U32  errsLen = 1;	// reserve index 0 for invalid
static U64  errFrame;
static char errStrBuf[MaxStrBuf];
static U64  errStrBufLen;

//--------------------------------------------------------------------------------------------------

static char* AllocStr(U64 len) {
	Assert(errStrBufLen + len <= MaxStrBuf);
	char* result = errStrBuf + errStrBufLen;
	errStrBufLen += len;
	return result;
}

//--------------------------------------------------------------------------------------------------

static Arg CloneArg(Arg arg) {
	if (arg.type != Arg::Type::Str) {
		return arg;
	}

	if (arg.s.len <= 256) {
		char* str = AllocStr(arg.s.len);
		memcpy(str, arg.s.data, arg.s.len);
		return Arg { .s = { .data = str, .len = arg.s.len } };
	}

	char* str = AllocStr(256);
	// 127 + "..." + 126
	memcpy(str, arg.s.data, 127);
	memcpy(str + 127, "...", 3);
	memcpy(str + 130, arg.s.data + arg.s.len - 126, 126);
	return Arg { .s = { .data = str, .len = 256 } };
}

//--------------------------------------------------------------------------------------------------

Err const* Err::Makev(Err const* prev, SrcLoc sl, Str ns, Str sCode, U64 uCode, Span<NamedArg const> namedArgs) {
	Assert(errsLen < MaxErrs);
	Assert(namedArgs.len <= Err::MaxNamedArgs);

	Err* const err = &errs[errsLen++];

	if (prev) {
		Assert(prev >= errs && prev < errs + errsLen);
		Assert(prev->frame == errFrame);
	}

	err->frame        = errFrame;
	err->prev         = prev;
	err->sl           = sl;
	err->ns           = ns;
	err->sCode        = sCode;
	err->uCode        = uCode;
	err->namedArgsLen = (U32)namedArgs.len;
	for (U64 i = 0; i < namedArgs.len; i++) {
		err->namedArgs[i] = {
			.name = namedArgs[i].name,
			.arg  = CloneArg(namedArgs[i].arg),
		};
	}

	return err;
}

//--------------------------------------------------------------------------------------------------

void Err::Frame(U64 frameIn) {
	errFrame = frameIn;
	memset(errs, 0, errsLen * sizeof(Err));
	errsLen = 0;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err