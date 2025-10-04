#include "JC/Err.h"

//--------------------------------------------------------------------------------------------------

static constexpr U32 Err_MaxDatas = 256;
static constexpr U32 Err_MaxStrBuf = 64 * 1024;

static Err_Data err_datas[Err_MaxDatas];
static U32      err_datasHead = 1;	// reserve index 0 for invalid
static U32      err_datasTail = Err_MaxDatas - 1;
static U64      err_frame;
static char     err_strBuf[Err_MaxStrBuf];
static U32      err_strBufLen;

//--------------------------------------------------------------------------------------------------

static Err_Data* Err_AllocData() {
	Assert(err_datasHead != err_datasTail);
	Err_Data* data = err_datas + err_datasHead;
	err_datasHead++;
	if (err_datasHead >= Err_MaxDatas) {
		err_datasHead = 1;
	}
	return data;
}

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

Err Err_Make(Err prev, SrcLoc sl, Str ns, Str code, Span<Str const> names, Span<Arg const> args) {
	Assert(names.len == args.len);
	Assert(names.len <= Err_MaxArgs);

	Err_Data* prevData = 0;
	if (prev.handle) {
		Assert(prev.handle < Err_MaxDatas);
		prevData = err_datas + prev.handle;
		Assert(prevData->frame == err_frame);
	}

	Err_Data* const data = Err_AllocData();
	data->frame   = err_frame;
	data->prev    = prevData;
	data->sl      = sl;
	data->ns      = ns;
	data->code    = code;
	data->argsLen = (U32)args.len;
	for (U64 i = 0; i < args.len; i++) {
		data->args[i].name = names[i];
		data->args[i].arg  = Err_CloneArg(args[i]);
	}

	return Err { .handle = (U64)(data - err_datas) };
}

//--------------------------------------------------------------------------------------------------

void Err_Frame(U64 frame) {
	err_frame = frame;
	if (err_datasHead == 1) {
		err_datasTail = Err_MaxDatas - 1;
	} else {
		err_datasTail = err_datasHead - 1;
	}
}