#include "JC/Err.h"
#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Fmt.h"
#include "JC/Sys.h"
#include <stdio.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct PanicApiImpl : PanicApi {
	[[noreturn]] void VPanic(s8 file, i32 line, s8 expr, s8 fmt, Args args) override {
		static bool recursive = false;
		if (recursive) {
			SysApi::Get()->Abort();
		}
		recursive = true;

		char msg[1024];
		char* begin = msg;
		char* end = msg + sizeof(msg) - 2;	// room for '\n' and '\0'
		begin = Fmt(begin, end,  "***Panic***\n");
		begin = Fmt(begin, end, "{}({})\n", file, line);
		if (expr != "") {
			begin = Fmt(begin, end, "{}\n", expr);
		}
		begin = VFmt(begin, end, fmt, args);
		if (end <= msg + sizeof(msg) - 2) {
			*end++ = '\n';
			*end = '\0';	// don't increment end to preserve the size
		} else {
			msg[sizeof(msg) - 1] = 0;
		}

		fwrite(msg, (u64)(end - begin), 1, stderr);

		if (DbgApi::Get()->IsPresent()) {
			DbgApi::Get()->Print(msg);
			DbgApi::Get()->Break();
		}

	}
};

static PanicApiImpl panicApiImpl;
static PanicApi*    panicApi = &panicApiImpl;

PanicApi* PanicApi::Get() {
	return panicApi;
}

PanicApi* PanicApi::Set(PanicApi* newPanicApi) {
	PanicApi* oldPanicApi = panicApi;
	panicApi = newPanicApi;
	return panicApi;
}
//--------------------------------------------------------------------------------------------------

static constexpr u32 MaxErrArgs = 32;

struct ErrArg {
	s8  name;
	Arg arg;
};

struct ErrObj {
	u64     prevIndex;
	s8      file;
	i32     line;
	ErrCode ec;
	ErrArg  args[MaxErrArgs];
	u32     argsLen;
};

struct ErrApiImpl : ErrApi {
	AllocatorApi* allocatorApi;
	Allocator*    allocator;
	Array<ErrObj> errObjs;

	void Init(AllocatorApi* inAllocatorApi) override {
		allocatorApi = inAllocatorApi;
		allocator    = allocatorApi->Create("err", 0);
		errObjs.Init(allocator);
		errObjs.Add();	// reserve index 0 for invalid
	}

	void Shutdown() override {
		errObjs.Shutdown();
		allocatorApi->Destroy(allocator);
	}

	Err Make(Err prev, s8 file, i32 line, ErrCode ec) override {
		JC_ASSERT(prev.handle < errObjs.len);
		ErrObj* errObj    = errObjs.Add();
		errObj->prevIndex = prev.handle;
		errObj->file      = file;
		errObj->line      = line;
		errObj->ec        = ec;
		errObj->argsLen   = 0;
		return Err { .handle = errObjs.len - 1 };
	}

	Err AddArg(Err err, s8 name, Arg arg) override {
		JC_ASSERT(err.handle > 0 && err.handle < errObjs.len);
		ErrObj* errObj = &errObjs.data[err.handle];
		JC_ASSERT(errObj->argsLen < MaxErrArgs);
		errObj->args[errObj->argsLen] = ErrArg { .name = name, .arg = arg };
		errObj->argsLen++;
		return err;
	}

	void Clear() override {
		errObjs.len = 0;
	}
};

static ErrApiImpl errApiImpl;

ErrApi* ErrApi::Get() {
	return &errApiImpl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC