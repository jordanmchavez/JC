#include "JC/Common_Err.h"
#include "JC/Common_Assert.h"

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxErrs   = 256;
static constexpr U32 MaxStrBuf = 64 * 1024;

static Err  errs[MaxErrs];
static U32  errsLen = 1;	// reserve index 0 for invalid
static U64  frame;
static char strBuf[MaxStrBuf];
static U64  strBufLen;

//--------------------------------------------------------------------------------------------------

static char* AllocStr(U64 len) {
	Assert(strBufLen + len <= MaxStrBuf);
	char* result = strBuf + strBufLen;
	strBufLen += len;
	return result;
}

//--------------------------------------------------------------------------------------------------

static Arg::Arg CloneArg(Arg::Arg arg) {
	if (arg.type != Arg::Type::Str) {
		return arg;
	}

	if (arg.s.len <= 256) {
		char* str = AllocStr(arg.s.len);
		memcpy(str, arg.s.data, arg.s.len);
		return Arg::Arg { .s = { .data = str, .len = arg.s.len } };
	}

	char* str = AllocStr(256);
	// 127 + "..." + 126
	memcpy(str, arg.s.data, 127);
	memcpy(str + 127, "...", 3);
	memcpy(str + 130, arg.s.data + arg.s.len - 126, 126);
	return Arg::Arg { .s = { .data = str, .len = 256 } };
}

//--------------------------------------------------------------------------------------------------

Err const* Makev(Err const* prev, SrcLoc sl, Str ns, Str sCode, U64 uCode, Span<NamedArg const> namedArgs) {
	Assert(errsLen < MaxErrs);
	Assert(namedArgs.len <= MaxNamedArgs);

	Err* const err = &errs[errsLen++];

	if (prev) {
		Assert(prev >= errs && prev < errs + errsLen);
		Assert(prev->frame == frame);
	}

	err->frame        = frame;
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

void Frame(U64 frame_) {
	frame = frame_;
	memset(errs, 0, errsLen * sizeof(Err));
	errsLen = 0;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err