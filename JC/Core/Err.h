#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct ErrCode {
	Str ns;
	Str code;
};

struct [[nodiscard]] Err {
	static constexpr U32 MaxArgs = 32;

	struct Data {
		Data*    prev               = {};
		SrcLoc   sl                 = {};
		Str      ns                 = {};
		Str      code               = {};
		NamedArg namedArgs[MaxArgs] = {};
		U32      namedArgsLen       = 0;
	};

	Data* data = 0;

	static void Init(Mem::TempAllocator* tempAllocator);

	Err() = default;

	template <class...A> Err(Str ns, Str code, A... args, SrcLoc sl) {
		NamedArg namedArgs[1 + (sizeof...(A) / 2)];
		BuildNamedArgs(namedArgs, args...);
		Init(ns, code, Span<NamedArg>(namedArgs, sizeof...(A) / 2), sl);
	}

	void Init(Str ns, Str code, Span<const NamedArg> namedArgs, SrcLoc sl);
	void Init(Str ns, U64 code, Span<const NamedArg> namedArgs, SrcLoc sl);

	Err Push(Err err);

	Str GetStr();
};

constexpr bool operator==(ErrCode ec, Err err) { return err.data && ec.ns == err.data->ns && ec.code == err.data->code; }
constexpr bool operator==(Err err, ErrCode ec) { return err.data && ec.ns == err.data->ns && ec.code == err.data->code; }

static_assert(sizeof(Err) == 8);

#define DefErr(InNs, InCode) \
	constexpr ErrCode EC_##InCode = { .ns = #InNs, .code = #InCode }; \
	template <class... A> struct Err_##InCode : JC::Err { \
		static inline U8 Sig = 0; \
		Err_##InCode(A... args, SrcLoc sl = SrcLoc::Here()) { \
			NamedArg namedArgs[1 + (sizeof...(A) / 2)]; \
			BuildNamedArgs(namedArgs, args...); \
			Init(#InNs, #InCode, Span<const NamedArg>(namedArgs, sizeof...(A) / 2), sl); \
		} \
	}; \
	template <class...A> Err_##InCode(A...) -> Err_##InCode<A...>

//--------------------------------------------------------------------------------------------------

}	// namespace JC