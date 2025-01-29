#pragma once

#include "JC/Core.h"

namespace JC::Err {

//--------------------------------------------------------------------------------------------------

void Init(Mem::TempAllocator* tempAllocator);

struct [[nodiscard]] Error {
	static constexpr u32 MaxArgs = 32;

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
		NamedArg namedArgs[1 + sizeof...(A) / 2];
		BuildNamedArgs(namedArgs, args...);
		Init(ns, code, Span<NamedArg>(namedArgs, sizeof...(A) / 2), sl);
	}

	void Init(Str ns, Str code, Span<NamedArg> namedArgs, SrcLoc sl);

	Error Push(Error error);
};

#define DefErr(Ns, Code) \
	template <class... A> struct Err_##Code : JC::Err::Error { \
		Err_##Code(A... args, SrcLoc sl = SrcLoc::Here()) { \
			NamedArg namedArgs[1 + sizeof...(A) / 2]; \
			BuildNamedArgs(namedArgs, args...); \
			Init(#Ns, #Code, Span<NamedArg>(namedArgs, sizeof...(A) / 2), sl); \
		} \
	}; \
	template <class...A> Err_##Code(A...) -> Err_##Code<A...>


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Err