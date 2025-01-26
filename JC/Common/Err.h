#pragma once

struct [[nodiscard]] Err {
	static constexpr u32 MaxNamedArgs = 32;

	struct NamedArg {
		Str  name = {};
		VArg varg  = {};
	};

	struct Data {
		Data*    prev                    = {};
		SrcLoc   sl                      = {};
		Str      ns                      = {};
		Str      code                    = {};
		NamedArg namedArgs[MaxNamedArgs] = {};
		u32      namedArgsLen            = 0;
	};

	Data* data;

	Err() = default;

	template <class...A> Err(SrcLoc sl, Str ns, Str code, VArgs vargs);

	template <class...A> Err(SrcLoc sl, Str ns, Str code, A... args) {
		static_assert(sizeof...(A) % 2 == 0);
		Err(sl, ns, 
		constexpr u64 NamedArgsLen = sizeof...(A) / 2;
		NamedArg namedArgs[NamedArgsLen > 0 ? NamedArgsLen : 1] = {};
		if constexpr (NamedArgsLen > 0) {
			BuildNamedArgsInternal(namedArgs, args...);
		}
		Init(sl, ns, code, Span<NamedArg>(namedArgs, NamedArgsLen));
	}

	void Init(SrcLoc sl, Str ns, Str code, Span<NamedArg> namedArgs);

	template <class T> static void BuildNamedArgsInternal(NamedArg* namedArgs, Str name, T arg) {
		namedArgs->name = name;
		namedArgs->varg = MakeVArg(arg);
	}

	template <class T, class... A> static void BuildNamedArgsInternal(NamedArg* namedArgs, Str name, T arg, A... args) {
		namedArgs->name = name;
		namedArgs->varg = MakeVArg(arg);
		BuildNamedArgsInternal(namedArgs + 1, args...);
	}

	Err Push(Err err);

	void AddInternal(Span<NamedArg> namedArgs);

	template <class... A> void Add(A... args) {
		static_assert(sizeof...(A) > 0 && sizeof...(A) % 2 == 0);
		constexpr u64 NamedArgsLen = sizeof...(A) / 2;
		NamedArg namedArgs[NamedArgsLen] = {};
		BuildNamedArgsInternal(namedArgs, args...);
		AddInternal(namedArgs);
	}
};

#define MakeErr(Ns, Code, ...) \
	MakeErrInternal(__FILE__, __LINE__, Ns, Code, __VA_ARGS__)
