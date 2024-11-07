#include "JC/Err.h"
#include "JC/Sys.h"
#include "JC/Unicode.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef CreateMutex

namespace JC {

//--------------------------------------------------------------------------------------------------
/*
s8 WinErrStr(u32 code) {
	LPWSTR wMsgBuf = nullptr;
	DWORD len = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&wMsgBuf,
		0,
		nullptr
	);
	if (len == 0) {
		return "";
	}

	while (len > 0 && wMsgBuf[len] == '\n' || wMsgBuf[len] == '\r' || wMsgBuf[len] == '.') {
		--len;
	}
	s8 msg = Unicode::Wtf16zToUtf8(s16z((const char16_t*)wMsgBuf, len));
	LocalFree(wMsgBuf);
	return msg;
}

template <class... A>
Err* WinErrImpl(s8 file, i32 line, ErrCode ec, u32 code, s8 fn, A... args) {
	Err* inner = Err::Make(nullptr, file, line, { .ns = "win", .code = code }, "msg", WinErrStr(code));
	return Err::Make(inner, file, line, ec, "fn", fn, args...);
}

#define WinErr(ec, code, fn, ...) WinErrImpl(__FILE__, __LINE__, ec, (code), (fn), __VA_ARGS__)
#define WinLastErr(ec, fn, ...) WinErrImpl(__FILE__, __LINE__, ec, GetLastError(), (fn), __VA_ARGS__)

s8 WinLastErrStr() { return WinErrStr(GetLastError()); }
*/
//--------------------------------------------------------------------------------------------------

static_assert(sizeof(SRWLOCK) == sizeof(Mutex));

struct ThreadApiImpl : ThreadApi {
	Mutex CreateMutex() override {
		SRWLOCK srwLock;
		InitializeSRWLock(&srwLock);
		return Mutex { .handle = *(u64*)&srwLock };
	}

	void LockMutex(Mutex m) override {
		AcquireSRWLockExclusive((SRWLOCK*)&m);
	}

	void UnlockMutex(Mutex m) override {
		ReleaseSRWLockExclusive((SRWLOCK*)&m);
	}

	void DestroyMutex(Mutex) override {
		// noop on windows
	}
};

static ThreadApiImpl threadApiImpl;

ThreadApi* ThreadApi::Init() {
	return &threadApiImpl;
}

//--------------------------------------------------------------------------------------------------

/*
i64 Time_JulianDays(i64 d, i32 m, i64 y) {
	// Fliegel, H. F. & van Flandern, T. C. 1968, Communications of the ACM, 11, 657.
	return d
		- 32075
		+ 1461 * (y + 4800 + (m - 14) / 12) / 4
		+ 367 * (m - 2 - (m - 14) / 12 * 12) / 12
		- 3 * ((y + 4900 + (m - 14) / 12) / 100) / 4
	;
}
*/

struct TimeApiImpl : TimeApi {
	static constexpr u64 TicksSince_1_1_1601 = 1892091424ull;
	static constexpr u64 TicksPerMillisecond = 10000;	// 10^-3 / 10^-7 = 10^4
	static constexpr u64 TicksPerSecond      = TicksPerMillisecond * 1000;
	static constexpr u64 TicksPerMinute      = TicksPerSecond * 60;
	static constexpr u64 TicksPerHour        = TicksPerMinute * 60;
	static constexpr u64 TicksPerDay         = TicksPerHour * 24;

	Time Now() override {
		FILETIME fileTime;
		GetSystemTimePreciseAsFileTime(&fileTime);
		return Time { .ticks = TicksSince_1_1_1601 + (((u64)fileTime.dwHighDateTime << 32) | (u64)fileTime.dwLowDateTime) };
	}

	Time Add(Time t, double seconds) override {
		return { .ticks = t.ticks + (u64)(seconds * TicksPerSecond) };
	}

	double Since(Time end, Time start) override {
		return (double)((end.ticks - start.ticks) * TicksPerSecond);
	}

	Date TimeToDate(Time t) override {
		// https://aa.usno.navy.mil/faq/JD_for mula
		// Fliegel, H. F. & van Flandern, T. C. 1968, Communications of the ACM, 11, 657.

		constexpr u64 TicksSince_1_1_1 = 1891507036ull;
		const u64 jd = TicksSince_1_1_1 + (t.ticks / TicksPerDay);
		u64 l = jd + 68569;
		u64 n = 4 * l / 146097;
			l = l - (146097 * n + 3) / 4;
		u64 i = 4000 * (l + 1) / 1461001;
			l = l - 1461 * i / 4 + 31;
		u64 j = 80 * l / 2447;
		u64 k = l - 2447 * j / 80;
			l = j / 11;
			j = j + 2 - 12 * l;
			i = 100 * (n - 49) + i + l;

		return Date {
			.year        = (u32)i,
			.month       = (u32)j,
			.day         = (u32)k,
			.hour        = (u32)((t.ticks / TicksPerHour       ) %   24),
			.minute      = (u32)((t.ticks / TicksPerMinute     ) %   60),
			.second      = (u32)((t.ticks / TicksPerSecond     ) %   60),
			.millisecond = (u32)((t.ticks / TicksPerMillisecond) % 1000),
		};
	}
};

static TimeApiImpl timeApiImpl;

TimeApi* TimeApi::Init() {
	return &timeApiImpl;
}

//--------------------------------------------------------------------------------------------------

/*static LONG ExceptionHandler(_EXCEPTION_POINTERS* ex) {
	(void)ex;
	return 1;
	//struct _EXCEPTION_POINTERS {
	//	PEXCEPTION_RECORD ExceptionRecord;
	//	PCONTEXT ContextRecord;
	//};
	//struct _EXCEPTION_RECORD {
	//	DWORD    ExceptionCode;
	//	DWORD ExceptionFlags;
	//	struct _EXCEPTION_RECORD *ExceptionRecord;
	//	PVOID ExceptionAddress;
	//	DWORD NumberParameters;
	//	ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
	//};

	//#define EXCEPTION_ACCESS_VIOLATION          STATUS_ACCESS_VIOLATION
	//#define EXCEPTION_DATATYPE_MISALIGNMENT     STATUS_DATATYPE_MISALIGNMENT
	//#define EXCEPTION_BREAKPOINT                STATUS_BREAKPOINT
	//#define EXCEPTION_SINGLE_STEP               STATUS_SINGLE_STEP
	//#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED     STATUS_ARRAY_BOUNDS_EXCEEDED
	//#define EXCEPTION_FLT_DENORMAL_OPERAND      STATUS_FLOAT_DENORMAL_OPERAND
	//#define EXCEPTION_FLT_DIVIDE_BY_ZERO        STATUS_FLOAT_DIVIDE_BY_ZERO
	//#define EXCEPTION_FLT_INEXACT_RESULT        STATUS_FLOAT_INEXACT_RESULT
	//#define EXCEPTION_FLT_INVALID_OPERATION     STATUS_FLOAT_INVALID_OPERATION
	//#define EXCEPTION_FLT_OVERFLOW              STATUS_FLOAT_OVERFLOW
	//#define EXCEPTION_FLT_STACK_CHECK           STATUS_FLOAT_STACK_CHECK
	//#define EXCEPTION_FLT_UNDERFLOW             STATUS_FLOAT_UNDERFLOW
	//#define EXCEPTION_INT_DIVIDE_BY_ZERO        STATUS_INTEGER_DIVIDE_BY_ZERO
	//#define EXCEPTION_INT_OVERFLOW              STATUS_INTEGER_OVERFLOW
	//#define EXCEPTION_PRIV_INSTRUCTION          STATUS_PRIVILEGED_INSTRUCTION
	//#define EXCEPTION_IN_PAGE_ERROR             STATUS_IN_PAGE_ERROR
	//#define EXCEPTION_ILLEGAL_INSTRUCTION       STATUS_ILLEGAL_INSTRUCTION
	//#define EXCEPTION_NONCONTINUABLE_EXCEPTION  STATUS_NONCONTINUABLE_EXCEPTION
	//#define EXCEPTION_STACK_OVERFLOW            STATUS_STACK_OVERFLOW
	//#define EXCEPTION_INVALID_DISPOSITION       STATUS_INVALID_DISPOSITION
	//#define EXCEPTION_GUARD_PAGE                STATUS_GUARD_PAGE_VIOLATION
	//#define EXCEPTION_INVALID_HANDLE            STATUS_INVALID_HANDLE
	//#define EXCEPTION_POSSIBLE_DEADLOCK         STATUS_POSSIBLE_DEADLOCK

	// return EXCEPTION_CONTINUE_EXECUTION;
	EXCEPTION_ACCESS_VIOLATION;
	return EXCEPTION_CONTINUE_SEARCH;
}
*/

//--------------------------------------------------------------------------------------------------

/*
Semaphore Semaphore_Create(u32 count) {
	HANDLE h = CreateSemaphoreW(nullptr, 0, (LONG)count, nullptr);
	Assert(h != nullptr);
	return Semaphore { .opaque = (u64)h };
}

void Semaphore_Destroy(Semaphore s) {
	CloseHandle((HANDLE)s.opaque);
}

void Semaphore_Wait(Semaphore s) {
	WaitForSingleObject((HANDLE)s.opaque, INFINITE);
}

void Semaphore_Release(Semaphore s, u32 count) {
	ReleaseSemaphore((HANDLE)s.opaque, (LONG)count, nullptr);
}
//--------------------------------------------------------------------------------------------------

struct Thread_Ctx {
	Thread_Fn* fn;
	void*      data;
	HANDLE     handle;
	u32        id;
};

static constexpr u32 Thread_MaxThreads = 128;

static Thread_Ctx thread_ctxs[Thread_MaxThreads] = { 0 };
static u32        thread_ctxsLen = 1;	// reserve 0 for invalid
static Mutex      thread_mtx;

static DWORD WINAPI Thread_Proc(void* data) {
	Thread_Ctx* ctx = (Thread_Ctx*)data;
	ctx->fn(ctx->data);
	return 0;
}

Thread Thread_Create(Thread_Fn fn, void* data, u64 stackSize) {
	u32 i = thread_ctxsLen.fetch_add(1);
	Assert(i < Thread_MaxThreads);
	Thread_Ctx* ctx = &thread_ctxs[i];
	ctx->fn = fn;
	ctx->data = data;

	DWORD threadId;
	HANDLE h = CreateThread(nullptr, stackSize, Thread_Proc, ctx, CREATE_SUSPENDED, &threadId);
	Assert(h != nullptr);
	ctx->handle = h;
	ctx->id = (u32)threadId;

	return Thread { .opaque = (u64)i };
}

void Thread_Wait(Thread t) {
	Assert(t.opaque);
	WaitForSingleObject(thread_ctxs[t.opaque].handle, INFINITE);
}

u32 Thread_Id(Thread t) {
	Assert(t.opaque);
	return thread_ctxs[t.opaque].id;
}
*/

//--------------------------------------------------------------------------------------------------

}	// namespace JC