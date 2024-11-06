#pragma once

#include "JC/Common.h"

namespace JC {

struct AllocatorApi;

//--------------------------------------------------------------------------------------------------

struct PanicApi {
	static PanicApi* Get();
	static PanicApi* Set(PanicApi* panicApi);

	[[noreturn]] virtual void VPanic(s8 file, i32 line, s8 expr, s8 fmt, Args args);

	[[noreturn]] void Panic(s8 file, i32 line, s8 expr) {
		VPanic(file, line, expr, "", Args::Make());
	}

	
	template <class... A> [[noreturn]] void Panic(s8 file, i32 line, s8 expr, s8 fmt, A... args) {
		VPanic(file, line, expr, fmt, Args::Make(args...));
	}
};

#define JC_ASSERT(expr, ...) \
	do { \
		if (!(expr)) { \
			PanicApi::Get()->Panic(__FILE__, __LINE__, #expr, __VA_ARGS__); \
		} \
	} while (false)

#define JC_PANIC(fmt, ...) \
	PanicApi::Get()->Panic(__FILE__, __LINE__, nullptr, (fmt), Args::Make(__VA_ARGS__))

//--------------------------------------------------------------------------------------------------

struct ErrApi {
	static ErrApi* Get();
	
	virtual void Init(AllocatorApi* allocatorApi);
	virtual void Shutdown();

	virtual Err  Make(Err prev, s8 file, i32 line, ErrCode ec) = 0;
	virtual Err  AddArg(Err err, s8 name, Arg arg) = 0;
	virtual void Clear() = 0;

	template <class... A> Err Make(Err prev, s8 file, i32 line, ErrCode ec, A... args) {
		Err err = Make(prev, file, line, ec);
		if constexpr (sizeof...(A) > 0) {
			return AddArgs(errApi, err, args...);
		}
		return *this;
	}

	template <class... A> Err AddArgs(Err err, s8 name, Arg arg, A... args) {
		static_assert(sizeof...(A) % 2 == 0);
		AddArg(err, name, Arg::Make(arg));
		if constexpr (sizeof...(A) > 0) {
			return AddArgs(err, args...);
		}
		return *this;
	}
};

#define JC_ERR(ec, ...) ErrApi::Get()->Make(Err(), __FILE__, __LINE__, ec, __VA_ARGS__)

//--------------------------------------------------------------------------------------------------

}	// namespace JC