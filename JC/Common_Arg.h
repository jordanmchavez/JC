#pragma once

#include "JC/Common_Std.h"

namespace JC::Arg {

//--------------------------------------------------------------------------------------------------

enum struct Type { Bool, Char, I64, U64, F64, Str, Ptr };

struct Arg {
	Type type;
	union {
		bool b;
		char c;
		I64 i;
		U64 u;
		F64 f;
		struct { char const* data; U64 len; } s;
		const void* p;
	};
};

constexpr Arg Make(bool               val) { return Arg { .type = Type::Bool, .b  = val }; }
constexpr Arg Make(char               val) { return Arg { .type = Type::Char, .c  = val }; }
constexpr Arg Make(signed char        val) { return Arg { .type = Type::I64,  .i  = val }; }
constexpr Arg Make(signed short       val) { return Arg { .type = Type::I64,  .i  = val }; }
constexpr Arg Make(signed int         val) { return Arg { .type = Type::I64,  .i  = val }; }
constexpr Arg Make(signed long        val) { return Arg { .type = Type::I64,  .i  = val }; }
constexpr Arg Make(signed long long   val) { return Arg { .type = Type::I64,  .i  = val }; }
constexpr Arg Make(unsigned char      val) { return Arg { .type = Type::U64,  .u  = val }; }
constexpr Arg Make(unsigned short     val) { return Arg { .type = Type::U64,  .u  = val }; }
constexpr Arg Make(unsigned int       val) { return Arg { .type = Type::U64,  .u  = val }; }
constexpr Arg Make(unsigned long      val) { return Arg { .type = Type::U64,  .u  = val }; }
constexpr Arg Make(unsigned long long val) { return Arg { .type = Type::U64,  .u  = val }; }
constexpr Arg Make(float              val) { return Arg { .type = Type::F64,  .f  = val }; }
constexpr Arg Make(double             val) { return Arg { .type = Type::F64,  .f  = val }; }
constexpr Arg Make(char*              val) { return Arg { .type = Type::Str,  .s  = { .data = val,      .len = val ? ConstExprStrLen(val) : 0 } }; }
constexpr Arg Make(char const*        val) { return Arg { .type = Type::Str,  .s  = { .data = val,      .len = val ? ConstExprStrLen(val) : 0 } }; }
constexpr Arg Make(Str                val) { return Arg { .type = Type::Str,  .s  = { .data = val.data, .len = val.len } }; }
constexpr Arg Make(const void*        val) { return Arg { .type = Type::Ptr,  .p  = val }; }
constexpr Arg Make(decltype(nullptr)  val) { return Arg { .type = Type::Ptr,  .p  = val }; }
constexpr Arg Make(Arg                val) { return val; }
							      
constexpr void Fill(Arg*) {}

template <class A1, class... A>
constexpr void Fill(Arg* out, A1 arg1, A... args) {
	*out = MakeArg(arg1);
	FillArgs(out + 1, args...);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Arg