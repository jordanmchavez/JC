#pragma once

#include "JC/Core.h"

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] Error {
	static constexpr u32 MaxArgs = 32;

	struct NamedArg {
		Str name = {};
		Arg arg  = {};
	};

	struct Data {
		Data*    prev               = {};
		SrcLoc   sl                 = {};
		Str      ns                 = {};
		Str      code               = {};
		NamedArg namedArgs[MaxArgs] = {};
		u32      namedArgsLen       = 0;
	};

	Data* data = 0;

	Error() = default;
	template <class...A> Error(Str ns, Str code, A... args, SrcLoc sl) {
		static_assert(sizeof...(A) % 2 == 0);
		Init(ns, code, Span<Arg>({ MakeArg(args...), }), sl);
	}
	void Init(Str ns, Str code, Span<Arg> args, SrcLoc sl);
	Error Push(Error error);
};

#define DefErr(Ns, Code) \
	struct Err_##Code : JC::Err::Error { \
		template <class... A> Err_##Code(A... args, SrcLoc sl = SrcLoc::Here()) { \
			static_assert(sizeof...(A) % 2 == 0); \
			Init(#Ns, Code, Span<Arg>({ MakeArg(args), ... }), sl); \
		} \
	}; \
	template <class...A> Err_##Code(A...) -> Err_##Code<A...>


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err