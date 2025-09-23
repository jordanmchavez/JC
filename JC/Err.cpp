#include "JC/Err.h"

#include "JC/Pool.h"

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxDatas = 256;
static constexpr U32 MaxStrBuf = 64 * 1024;

static HandlePool<Data, Err, MaxDatas> dataPool;
static U64                             frame;
static char                            strBuf[MaxStrBuf];
static U32                             strBufLen;

//--------------------------------------------------------------------------------------------------

static char* AllocStr(U64 len) {
	Assert(strBufLen + len <= MaxStrBuf);
	char* result = strBuf + strBufLen;
	strBufLen += len;
	return result;
}

//--------------------------------------------------------------------------------------------------

static Arg CloneArg(Arg arg) {
	if (arg.type != ArgType::Str) {
		return arg;
	}

	if (arg.s.l <= 256) {
		char* str = AllocStr(arg.s.l);
		memcpy(str, arg.s.s, arg.s.l);
		return Arg { .s = { .s = str, .l = arg.s.l } };
	}

	char* str = AllocStr(256);
	// 127 + "..." + 126
	memcpy(str, arg.s.s, 127);
	memcpy(str + 127, "...", 3);
	memcpy(str + 130, arg.s.s + arg.s.l - 126, 126);
	return Arg { .s = { .s = str, .l = 256 } };
}

//--------------------------------------------------------------------------------------------------

Err Err::Make(Err prev, SrcLoc sl, Str ns, Str code, Span<const NamedArg> namedArgs) {
	JC_ASSERT(namedArgs.len <= ErrData::MaxNamedArgs);

	ErrData* const errData = AllocErrData();
	errData->frame        = frame;
	errData->prev         = prev.data;
	errData->sl           = sl;
	errData->ns           = ns;
	errData->code         = code;
	errData->namedArgsLen = namedArgs.len;
	for (U64 i = 0; i < namedArgs.len; i++) {
		errData->namedArgs[i].name = namedArgs[i].name;
		errData->namedArgs[i].arg  = CloneArg(namedArgs[i].arg);
	}

	return Err { .data = errData };
}

//--------------------------------------------------------------------------------------------------

void Frame(U64 frame_) {
	frame = frame_;
	errDatasTail = (errDatasHead > 0) ? (errDatasHead - 1) : (MaxErrDatas - 1);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err