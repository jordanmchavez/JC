#include "JC/Err.h"
#include "JC/Sys.h"

//--------------------------------------------------------------------------------------------------

static constexpr U32 Err_MaxDatas = 256;
static constexpr U32 Err_MaxStrBuf = 64 * 1024;

static Err_Data err_datas[Err_MaxDatas];
static U32      err_datasLen = 1;	// reserve index 0 for invalid
static U64      err_frame;
static char     err_strBuf[Err_MaxStrBuf];
static U32      err_strBufLen;

//--------------------------------------------------------------------------------------------------

static char* Err_AllocStr(U32 len) {
	Assert(err_strBufLen + len <= Err_MaxStrBuf);
	char* result = err_strBuf + err_strBufLen;
	err_strBufLen += len;
	return result;
}

//--------------------------------------------------------------------------------------------------

static Arg Err_CloneArg(Arg arg) {
	if (arg.type != ArgType::Str) {
		return arg;
	}

	if (arg.s.l <= 256) {
		char* str = Err_AllocStr(arg.s.l);
		memcpy(str, arg.s.s, arg.s.l);
		return Arg { .s = { .s = str, .l = arg.s.l } };
	}

	char* str = Err_AllocStr(256);
	// 127 + "..." + 126
	memcpy(str, arg.s.s, 127);
	memcpy(str + 127, "...", 3);
	memcpy(str + 130, arg.s.s + arg.s.l - 126, 126);
	return Arg { .s = { .s = str, .l = 256 } };
}

//--------------------------------------------------------------------------------------------------

void Err_AddArg(Err err, Str name, Arg arg) {
	Assert(err.handle > 0 && err.handle < err_datasLen);
	Err_Data* const data = &err_datas[err.handle];
	Assert(data->argsLen < Err_MaxArgs);
	data->args[data->argsLen] = {
		.name = name,
		.arg  = Err_CloneArg(arg),
	};
}

//--------------------------------------------------------------------------------------------------

Err Err_Make(Err prev, SrcLoc sl, Str ns, U64 code) {
	Assert(err_datasLen < Err_MaxDatas);
	Err_Data* const data = &err_datas[err_datasLen++];

	Err_Data* prevData = 0;
	if (prev.handle) {
		Assert(prev.handle < err_datasLen);
		prevData = &err_datas[prev.handle];
		Assert(prevData->frame == err_frame);
	}

	data->frame   = err_frame;
	data->prev    = prevData;
	data->sl      = sl;
	data->ns      = ns;
	data->uCode   = code;
	data->argsLen = 0;

	#if defined BreakOnError
		if (Sys_DbgPresent()) {
			Dbg_Break;
		}
	#endif	// BreakOnError

	return Err { .handle = (U64)(data - err_datas) };
}

Err Err_Make(Err prev, SrcLoc sl, Str ns, Str code) {
	Assert(err_datasLen < Err_MaxDatas);
	Err_Data* const data = &err_datas[err_datasLen++];

	Err_Data* prevData = 0;
	if (prev.handle) {
		Assert(prev.handle < err_datasLen);
		prevData = &err_datas[prev.handle];
		Assert(prevData->frame == err_frame);
	}

	data->frame   = err_frame;
	data->prev    = prevData;
	data->sl      = sl;
	data->ns      = ns;
	data->sCode   = code;
	data->argsLen = 0;

	#if defined BreakOnError
		if (Sys_DbgPresent()) {
			Dbg_Break;
		}
	#endif	// BreakOnError

	return Err { .handle = (U64)(data - err_datas) };
}

//--------------------------------------------------------------------------------------------------

Err_Data const* Err_GetData(Err err) {
	Assert(err.handle > 0 && err.handle < err_datasLen);
	return &err_datas[err.handle];
	
}

//--------------------------------------------------------------------------------------------------

void Err_Frame(U64 frame) {
	err_frame = frame;
	memset(err_datas, 0, err_datasLen * sizeof(Err_Data));
	err_datasLen = 0;
}