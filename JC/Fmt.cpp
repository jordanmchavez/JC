#include "JC/Fmt.h"

#include "JC/Array.h"
#include "JC/Mem.h"
#include "JC/Unit.h"
#include <math.h>
#include "3rd/dragonbox/dragonbox.h"

//--------------------------------------------------------------------------------------------------

static constexpr U32 Fmt_Left  = 1 << 0;
static constexpr U32 Fmt_Plus  = 1 << 1;
static constexpr U32 Fmt_Space = 1 << 2;
static constexpr U32 Fmt_Zero  = 1 << 3;
static constexpr U32 Fmt_Hex   = 1 << 4;
static constexpr U32 Fmt_Bin   = 1 << 5;
static constexpr U32 Fmt_Fix   = 1 << 6;
static constexpr U32 Fmt_Sci   = 1 << 7;
static constexpr U32 Fmt_Upper = 1 << 8;

//--------------------------------------------------------------------------------------------------

struct Fmt_FixedOut {
	char* begin;
	char* end;

	void Add(char c) {
		if (end < begin) {
			*begin++ = c;
		}
	}

	void Add(char const* str, U32 strLen) {
		strLen = Min((U32)(end - begin), strLen);
		memcpy(begin, str, strLen);
		begin += strLen;
	}

	void Fill(char c, U32 n) {
		n = Min((U32)(end - begin), n);
		memset(begin, c, n);
		begin += n;
	}
};

//--------------------------------------------------------------------------------------------------

static constexpr char const* Fmt_Tens = 
	"00" "01" "02" "03" "04" "05" "06" "07" "08" "09"
	"10" "11" "12" "13" "14" "15" "16" "17" "18" "19"
	"20" "21" "22" "23" "24" "25" "26" "27" "28" "29"
	"30" "31" "32" "33" "34" "35" "36" "37" "38" "39"
	"40" "41" "42" "43" "44" "45" "46" "47" "48" "49"
	"50" "51" "52" "53" "54" "55" "56" "57" "58" "59"
	"60" "61" "62" "63" "64" "65" "66" "67" "68" "69"
	"70" "71" "72" "73" "74" "75" "76" "77" "78" "79"
	"80" "81" "82" "83" "84" "85" "86" "87" "88" "89"
	"90" "91" "92" "93" "94" "95" "96" "97" "98" "99";

static Str Fmt_U64ToDigits(U64 u, char* out, U32 outLen) {
	// TODO: test and profile splitting into two 32-bit ints to avoid 64-bit divides
	char* const end = out + outLen;
	char* iter = end;
	while (u >= 100) {
		iter -= 2;
		*(U16*)iter = *(U16*)&Fmt_Tens[(u % 100) * 2];
		u /= 100;
	}
	if (u >= 10) {
		iter -= 2;
		*(U16*)iter = *(U16*)&Fmt_Tens[u * 2];
	}
	else
	{
		*--iter = '0' + (char)u;
	}
	return Str(iter, (U32)(end - iter));
}

static Str Fmt_U64ToHexits(U64 u, char* out, U32 outLen, bool upper) {
	char* const end = out + outLen;
	char* iter = end;
	constexpr char const* hexitsLower = "0123456789abcdef";
	constexpr char const* hexitsUpper = "0123456789ABCDEF";
	char const* hexits = upper ? hexitsUpper : hexitsLower;
	do {
		*--iter = hexits[u & 0xf];
		u >>= 4;
	} while (u > 0);
	return Str(iter, (U32)(end - iter));
}

static Str Fmt_U64ToBits(U64 u, char* out, U32 outLen) {
	char* const end = out + outLen;
	char* iter = end;
	constexpr char const* bits = "01";
	do {
		*--iter = bits[u & 0x1];
		u >>= 1;
	} while (u > 0);
	return Str(iter, (U32)(end - iter));
}

//--------------------------------------------------------------------------------------------------	

// This function assumes there's a space right before ptr to hold the possible rounding character
static Bool Fmt_Round(char* ptr, U64 len, U64 round) {
	char const rem = ptr[round];
	if (
		(rem < '5') ||
		((rem == '5') && (round == len - 1) && !(ptr[round - 1] - '0' & 1))
	) {
		return false;
	}

	char* iter = ptr + round - 1;
	while (iter >= ptr) {
		if (*iter < '9') {
			++*iter;
			return false;
		}
		*iter = '0';
		--iter;
	}
	return true;
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
static void Fmt_PrintStr(Out* out, Str str, U32 flags, U32 width, U32 prec) {
	if (prec && str.len > prec) {
		str.len = prec;
	}
	const U32 pad = (width > (U32)str.len) ? width - (U32)str.len : 0;
	Bool left = flags & Fmt_Left;
	if (!left) {
		out->Fill(' ', pad);
	}
	out->Add(str.data, str.len);
	if (left) {
		out->Fill(' ', pad);
	}
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
static void Fmt_PrintI64(Out* out, I64 i, U32 flags, U32 width, U32) {	// prec unused
	char sign;
	U32 totalLen = 0;
	     if (i < 0)              { i = -i; sign = '-'; totalLen = 1; }
	else if (flags & Fmt_Plus)  {         sign = '+'; totalLen = 1; }
	else if (flags & Fmt_Space) {         sign = ' '; totalLen = 1; }
	else                         {         sign = '\0'; }

	char buf[72];
	const Str str = Fmt_U64ToDigits((U64)i, buf, sizeof(buf));
	totalLen += (U32)str.len;
	const U32 pad = (width > totalLen) ? width - totalLen : 0;
	U32 zeros = 0;
	if (!(flags & Fmt_Left)) {
		if (flags & Fmt_Zero) { zeros = pad; }
		else { out->Fill(' ', pad); }
	}
	if (sign) { out->Add(sign); }
	out->Fill('0', zeros);
	out->Add(str.data, str.len);
	if (flags & Fmt_Left) { out->Fill(' ', pad); }
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
static void Fmt_PrintU64(Out* out, U64 u, U32 flags, U32 width, U32) {	// prec unused
	char buf[72];
	Str str;
	     if (flags & Fmt_Hex) { str = Fmt_U64ToHexits(u, buf, sizeof(buf), flags & Fmt_Upper); }
	else if (flags & Fmt_Bin) { str = Fmt_U64ToBits  (u, buf, sizeof(buf)); }
	else                      { str = Fmt_U64ToDigits(u, buf, sizeof(buf)); }
	const U32 pad = (width > (U32)str.len) ? width - (U32)str.len : 0;
	if (!(flags & Fmt_Left)) {
		out->Fill((flags & Fmt_Zero) ? '0' : ' ', pad);
	}
	out->Add(str.data, str.len);
	if (flags & Fmt_Left) {
		out->Fill(' ', pad);
	}
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
static void Fmt_PrintF64(Out* out, F64 f, U32 flags, U32 width, U32 prec) {
	char sign;
	if (signbit(f))              { sign = '-'; f = -f; }
	else if (flags & Fmt_Plus)  { sign = '+'; }
	else if (flags & Fmt_Space) { sign = ' '; }
	else                         { sign = '\0'; }

	if (const int fpc = fpclassify(f); fpc == FP_INFINITE || fpc == FP_NAN) {
		const Str str = (fpc == FP_INFINITE) ? "inf" : "nan";
		const U32 len = (sign ? 1 : 0) + 3;
		const U32 pad = (width > len) ? width - len: 0;
		if (!(flags & Fmt_Left)) { out->Fill(' ', pad); }
		if (sign) { out->Add(sign); }
		out->Add(str.data, str.len);
		if (flags & Fmt_Left) { out->Fill(' ', pad); }
		return;
	}

	constexpr U32 MaxSigDigs = 17;
	char sigBuf[MaxSigDigs + 1] = "";
	Str  sigStr;
	U32  intDigs        = 0;
	U32  intTrailZeros  = 0;
	U32  fracLeadZeros  = 0;
	U32  fracDigs       = 0;
	U32  fracTrailZeros = 0;
	I32  exp            = 0;
	char dispExpSign    = '\0';
	I32  dispExp        = 0;

	if (f == 0.0) {
		sigBuf[0] = '0';
		sigStr.data = sigBuf;
		sigStr.len = 1;
		exp = 0;
	} else {
		const auto db = jkj::dragonbox::to_decimal(
			f,
			jkj::dragonbox::policy::sign::ignore,
			jkj::dragonbox::policy::cache::compact,
			jkj::dragonbox::policy::trailing_zero::remove,
			jkj::dragonbox::policy::binary_to_decimal_rounding::to_even
		);
		sigStr = Fmt_U64ToDigits(db.significand, sigBuf + 1, sizeof(sigBuf) - 1);
		exp = db.exponent;
	}

	const Bool sci = (flags & Fmt_Sci) || (!(flags & Fmt_Fix) && (exp >= 6 || ((I32)sigStr.len + exp) <= -4));
	if (sci) {
		intDigs = 1u;
		fracLeadZeros = (sigStr.len == 1u) ? 1u : 0u;
		fracDigs = (U32)(sigStr.len - 1u);
		dispExp = exp + (I32)sigStr.len - 1;
		if (dispExp < 0) {
			dispExp = -dispExp;
			dispExpSign = '-';
		}

	// 12345e3 -> 12345000.0
	} else if (exp >= 0) {
		intDigs = (U32)sigStr.len;
		intTrailZeros = (U32)exp;
		fracTrailZeros = (prec == 0) ? 1 : prec;

	// 12345e-2 -> 123.45
	} else if (sigStr.len > (U64)(-exp)) {
		intDigs = (U32)((I32)sigStr.len + exp);
		fracDigs = (U32)(-exp);

	// 12345e-9 -> 0.000012345
	// 99999e-5 -> 0.99999
	} else {	// sigDigs <= -exp
		intDigs = 0;
		intTrailZeros = 1;
		fracLeadZeros = (U32)(-exp) - (U32)sigStr.len;
		fracDigs = (U32)sigStr.len;
	}

	if (prec > 0) {
		if (prec <= fracLeadZeros) {
			fracDigs = 0;
			fracLeadZeros = prec;
		} else if (prec < fracLeadZeros + fracDigs) {
			fracDigs = prec - fracLeadZeros;
			if (Fmt_Round((char*)sigStr.data, sigStr.len, intDigs + fracDigs)) {
				*((char*)--sigStr.data) = '1';
				if (sci) {
					++exp;
				} else if (intDigs == 0 && intTrailZeros == 1) {
					intDigs = 1;
					intTrailZeros = 0;
				} else {
					++intDigs;
				}
			}
		} else if (prec > fracLeadZeros + fracDigs) {
			fracTrailZeros = prec - fracLeadZeros - fracDigs;
		}
	}

	U32 totalLen = (sign ? 1 : 0) + intDigs + intTrailZeros + 1 + fracLeadZeros + fracDigs + fracTrailZeros;
	if (sci) {
		totalLen += 1 + (dispExpSign ? 1 : 0);
		     if (exp <=   9) { totalLen += 1; }
		else if (exp <=  99) { totalLen += 2; }
		else if (exp <= 999) { totalLen += 3; }
		else                 { totalLen += 4; }
	}
	const U32 pad = (width > totalLen) ? width - totalLen : 0;

	U32 intLeadZeros = 0;
	if (!(flags & Fmt_Left)) {
		if (flags & Fmt_Zero) {
			intLeadZeros = pad;
		} else {
			out->Fill(' ', pad);
		}
	}
	if (sign) { out->Add(sign); }
	out->Fill('0', intLeadZeros);
	out->Add(sigStr.data, intDigs);
	out->Fill('0', intTrailZeros);
	out->Add('.');
	out->Fill('0', fracLeadZeros);
	out->Add(sigStr.data + intDigs, fracDigs);
	out->Fill('0', fracTrailZeros);
	if (sci) {
		out->Add((flags & Fmt_Upper) ? 'E' : 'e');
		if (dispExpSign) {
			out->Add('-');
		}
		I32 de = dispExp;	// local copy for mutation
		if (de >= 100) {
			char const* const ten = &Fmt_Tens[(U64)(de / 100) * 2];
			if (de >= 10000) {
				out->Add(ten[0]);
			}
			out->Add(ten[1]);
			de %= 100;
		}
		char const* const ten = &Fmt_Tens[(U64)de * 2];
		if (dispExp >= 10) {
			out->Add(ten[0]);
		}
		out->Add(ten[1]);
	}
	if (flags & Fmt_Left) {
		out->Fill(' ', pad);
	}
}

//--------------------------------------------------------------------------------------------------

template <class Out>
static void Fmt_PrintPtr(Out* out, const void* p, U32 flags, U32 width, U32) {	// prec unused
	char buf[16];
	char* bufIter = buf + sizeof(buf);
	U64 u = (U64)p;
	for (U32 i = 0; i < 16; i++) {
		*--bufIter= "0123456789abcdef"[u & 0xf];
		u >>= 4;
	}

	constexpr U32 totalLen = 18;
	const U32 pad = (width > totalLen) ? width - totalLen : 0;
	U32 zeros = 0;
	if (!(flags & Fmt_Left)) {
		if (flags & Fmt_Zero) { zeros = pad; }
		else { out->Fill(' ', pad); }
	}
	out->Add("0x", 2);
	out->Fill('0', zeros);
	out->Add(buf, 16);
	if (flags & Fmt_Left) { out->Fill(' ', pad); }
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
void Fmt_PrintImpl(Out* out, char const* fmt, Span<const Arg> args) {
	U32 argIdx = 0;

	for (;;) {
		char const* start = fmt;
		char const* f = fmt;
		while (*f != '%') {
			if (*f == 0) {
				Assert(argIdx == args.len);
				out->Add(start, (U32)(f - start));
				return;
			}
			f++;
		}
		out->Add(start, (U32)(f - start));
		f++;

		U32 flags = 0;
		for (;;) {
			switch (*f) {
				case '-': flags |= Fmt_Left;  f++; continue;
				case '+': flags |= Fmt_Plus;  f++; continue;
				case ' ': flags |= Fmt_Space; f++; continue;
				case '0': flags |= Fmt_Zero;  f++; goto FlagsDone;
				default: goto FlagsDone;
			}
		}

		FlagsDone:
		U32 width = 0;
		while (*f >= '0' && *f <= '9') {
			width = (width * 10) + (*f - '0');
			f++;
		}

		I32 prec = -1;
		if (*f == '.') {
			f++;
			while (*f >= '0' && *f <= '9') {
				prec = (prec * 10) + (*f - '0');
				f++;
			}
		}

		Assert(argIdx < args.len);
		Arg const* arg = &args[argIdx];
		switch (*f) {
			#define Fmt_PrintfCase(ch, ExpectedArgType, Fn, val, addlFlags) \
				case ch: \
					Assert(arg->type == ExpectedArgType); \
					Fn(out, val, flags | addlFlags, width, prec); \
					break;
			Fmt_PrintfCase('t', ArgType::Bool, Fmt_PrintStr, arg->b ? "true" : "false", 0);
			Fmt_PrintfCase('c', ArgType::Char, Fmt_PrintStr, Str(&arg->c, 1),           0);
			Fmt_PrintfCase('s', ArgType::Str,  Fmt_PrintStr, Str(arg->s.s, arg->s.l),   0);
				// prec unusedFmt_PrintfCase('i', ArgType::I64,  Fmt_PrintI64, arg->i,                    0);
			Fmt_PrintfCase('u', ArgType::U64,  Fmt_PrintU64, arg->u,                    0);
			Fmt_PrintfCase('x', ArgType::U64,  Fmt_PrintU64, arg->u,                    Fmt_Hex);
			Fmt_PrintfCase('X', ArgType::U64,  Fmt_PrintU64, arg->u,                    Fmt_Hex | Fmt_Upper);
			Fmt_PrintfCase('b', ArgType::U64,  Fmt_PrintU64, arg->u,                    Fmt_Bin);
			Fmt_PrintfCase('f', ArgType::F64,  Fmt_PrintF64, arg->f,                    Fmt_Fix);
			Fmt_PrintfCase('e', ArgType::F64,  Fmt_PrintF64, arg->f,                    Fmt_Sci);
			Fmt_PrintfCase('E', ArgType::F64,  Fmt_PrintF64, arg->f,                    Fmt_Sci | Fmt_Upper);
			Fmt_PrintfCase('g', ArgType::F64,  Fmt_PrintF64, arg->f,                    0);
			Fmt_PrintfCase('p', ArgType::Ptr,  Fmt_PrintPtr, arg->p,                    0);
			#undef Fmt_PrintfCase

			default: Panic("Unhandled format type %c", *f);
		}
		f++;
	}
}

//--------------------------------------------------------------------------------------------------	

Str Fmt_Printv(Mem* mem, char const* fmt, Span<Arg const> args) {
	Array<char> arr(mem);
	Fmt_PrintImpl(&arr, fmt, args);
	return Str(arr.data, (U32)arr.len);

}

void Fmt_Printv(Array<char>* arr, char const* fmt, Span<Arg const> args) {
	Fmt_PrintImpl(arr, fmt, args);

}

char* Fmt_Printv(char* outBegin, char* outEnd, char const* fmt, Span<Arg const> args) {
	Fmt_FixedOut fixedOut = { .begin = outBegin, .end = outEnd };
	Fmt_PrintImpl(&fixedOut, fmt, args);
	return fixedOut.begin;
}

//--------------------------------------------------------------------------------------------------	

/*
UnitTest("Fmt") {
	#define CheckPrintf(expect, fmt, ...) { CheckEq(expect, Printf(testAllocator, fmt, ##__VA_ARGS__)); }

	// Escape sequences
	CheckPrintf("{", "{{");
	CheckPrintf("{", "{{");
	CheckPrintf("before {", "before {{");
	CheckPrintf("{ after", "{{ after");
	CheckPrintf("before { after", "before {{ after");
	CheckPrintf("}", "}}");
	CheckPrintf("before }", "before }}");
	CheckPrintf("} after", "}} after");
	CheckPrintf("before } after", "before }} after");
	CheckPrintf("{}", "{{}}");
	CheckPrintf("{42}", "{{{}}}", 42);

	// Basic args
	CheckPrintf("abc", "{}{}{}", 'a', 'b', 'c');
	CheckPrintf("a 1 b -5 c xyz", "a {} b {} c {}", 1, -5, "xyz");

	// Left align
	CheckPrintf("42  ", "{<4}", 42);
	CheckPrintf("42  ", "{<4x}", 0x42u);
	CheckPrintf("-42  ", "{<5}", -42);
	CheckPrintf("42   ", "{<5}", 42u);
	CheckPrintf("-42  ", "{<5}", -42l);
	CheckPrintf("42   ", "{<5}", 42ul);
	CheckPrintf("-42  ", "{<5}", -42ll);
	CheckPrintf("42   ", "{<5}", 42ull);
	CheckPrintf("-42.0  ", "{<7}", -42.0);
	CheckPrintf("true   ", "{<7}", true);
	CheckPrintf("c    ", "{<5}", 'c');
	CheckPrintf("123456789", "{<5}", "123456789");
	CheckPrintf("abc  ", "{<5}", "abc");
	CheckPrintf("0x12345678abcdef12  ", "{<20}", reinterpret_cast<void*>(0x12345678abcdef12));

	// Sign '+'
	CheckPrintf("+42", "{+}", 42);
	CheckPrintf("-42", "{+}", -42);
	CheckPrintf("+42", "{+}", 42);
	CheckPrintf("+42", "{+}", 42l);
	CheckPrintf("+42", "{+}", 42ll);
	CheckPrintf("+42.0", "{+}", 42.0);

	// Sign ' '
	CheckPrintf(" 42", "{ }", 42);
	CheckPrintf("-42", "{ }", -42);
	CheckPrintf(" 42", "{ }", 42);
	CheckPrintf(" 42", "{ }", 42l);
	CheckPrintf(" 42", "{ }", 42ll);
	CheckPrintf(" 42.0", "{ }", 42.0);
	CheckPrintf("-42.0", "{ }", -42.0);

	// Zero-padding '0'
	CheckPrintf("42", "{0}", 42);
	CheckPrintf("-0042", "{05}", -42);
	CheckPrintf("00042", "{05}", 42u);
	CheckPrintf("-0042", "{05}", -42l);
	CheckPrintf("00042", "{05}", 42ul);
	CheckPrintf("-0042", "{05}", -42ll);
	CheckPrintf("00042", "{05}", 42ull);
	CheckPrintf("0000042.0", "{09}",  42.0);
	CheckPrintf("-000042.0", "{09}", -42.0);

	// Width
	CheckPrintf(" -42", "{4}", -42);
	CheckPrintf("   42", "{5}", 42u);
	CheckPrintf("   -42", "{6}", -42l);
	CheckPrintf("     42", "{7}", 42ul);
	CheckPrintf("   -42", "{6}", -42ll);
	CheckPrintf("     42", "{7}", 42ull);
	CheckPrintf("   -1.23", "{8}", -1.23);
	CheckPrintf("  0x12345678abcdef12", "{20}", reinterpret_cast<void*>(0x12345678abcdef12));
	CheckPrintf("       true", "{11}", true);
	CheckPrintf("          x", "{11}", 'x');
	CheckPrintf("         str", "{12}", "str");
	CheckPrintf("abcdef", "{5}", "abcdef");
	CheckPrintf("abcdef", "{4}", "abcdef");
	CheckPrintf("0000.0", "{06.1}", 0.00884311);

	// Precision
	CheckPrintf("1.23", "{.2}", 1.2345);
	CheckPrintf("1.23e56", "{.2}", 1.234e56);
	CheckPrintf("1.100", "{.3}", 1.1);
	CheckPrintf("1.00e0", "{.2e}", 1.0);
	CheckPrintf("000000.000e0", "{012.3e}", 0.0);
	CheckPrintf("123.5", "{.1}", 123.456);
	CheckPrintf("123.46", "{.2}", 123.456);
	CheckPrintf("1.23", "{.000002}", 1.234);
	CheckPrintf("1019666432.0", "{}", 1019666432.0f);
	CheckPrintf("9.6e0", "{.1e}", 9.57);
	CheckPrintf("1.00e-34", "{.2e}", 1e-34);

	// Bool
	CheckPrintf("true", "{}", true);
	CheckPrintf("false", "{}", false);
	CheckPrintf("true ", "{<5}", true);
	CheckPrintf(" false", "{6}", false);

	// Short
	CheckPrintf("42", "{}", (short)42);
	CheckPrintf("42", "{}", (unsigned short)42);

	// Binary
	CheckPrintf("0", "{b}", 0u);
	CheckPrintf("101010", "{b}", 42u);
	CheckPrintf("11000000111001", "{b}", 12345u);
	CheckPrintf("10010001101000101011001111000", "{b}", 0x12345678u);
	CheckPrintf("10010000101010111100110111101111", "{b}", 0x90abcdefu);
	CheckPrintf("11111111111111111111111111111111", "{b}", 0xffffffffu);

	// Decimal
	CheckPrintf("0", "{}", 0);
	CheckPrintf("42", "{}", 42);
	CheckPrintf("42", "{}", 42u);
	CheckPrintf("-42", "{}", -42);
	CheckPrintf("12345", "{}", 12345);
	CheckPrintf("67890", "{}", 67890);

	// TODO: INT_MIN, ULONG_MAX, etc for hex/bin/dec
	// TODO: check unknown types
	// TODO: test with maxint as the precision literal

	// Hex
	CheckPrintf("0", "{x}", 0u);
	CheckPrintf("42", "{x}", 0x42u);
	CheckPrintf("12345678", "{x}", 0x12345678u);
	CheckPrintf("90abcdef", "{x}", 0x90abcdefu);
	
	// Float
	CheckPrintf("0.0", "{}", 0.0f);
	CheckPrintf("392.5", "{}", 392.5f);

	// Double
	CheckPrintf("0.0", "{}", 0.0);
	CheckPrintf("392.65", "{}", 392.65);
	CheckPrintf("3.9265e2", "{e}", 392.65);
	CheckPrintf("4901400.0", "{}", 4.9014e6);
	CheckPrintf("+00392.6500", "{+011.4}", 392.65);
	CheckPrintf("9223372036854776000.0", "{}", 9223372036854775807.0);

	// Precision rounding
	CheckPrintf("0.000", "{.3f}", 0.00049);
	CheckPrintf("0.000", "{.3f}", 0.0005);
	CheckPrintf("0.002", "{.3f}", 0.0015);
	CheckPrintf("0.001", "{.3f}", 0.00149);
	CheckPrintf("0.002", "{.3f}", 0.0015);
	CheckPrintf("1.000", "{.3f}", 0.9999);
	CheckPrintf("0.001", "{.3}", 0.00123);
	CheckPrintf("0.1000000000000000", "{.16}", 0.1);
	CheckPrintf("225.51575035152064000", "{.17f}", 225.51575035152064);
	CheckPrintf("-761519619559038.2", "{.1f}", -761519619559038.2);
	CheckPrintf("1.9156918820264798e-56", "{}", 1.9156918820264798e-56);
	CheckPrintf("0.0000", "{.4f}", 7.2809479766055470e-15);
	CheckPrintf("3788512123356.9854", "{f}", 3788512123356.985352);

	// Float formatting
	CheckPrintf("0.001", "{}", 1e-3);
	CheckPrintf("0.0001", "{}", 1e-4);
	CheckPrintf("1.0e-5", "{}", 1e-5);
	CheckPrintf("1.0e-6", "{}", 1e-6);
	CheckPrintf("1.0e-7", "{}", 1e-7);
	CheckPrintf("1.0e-8", "{}", 1e-8);
	CheckPrintf("1.0e15", "{}", 1e15);
	CheckPrintf("1.0e16", "{}", 1e16);
	CheckPrintf("9.999e-5", "{}", 9.999e-5);
	CheckPrintf("1.234e10", "{}", 1234e7);
	CheckPrintf("12.34", "{}", 1234e-2);
	CheckPrintf("0.001234", "{}", 1234e-6);
	CheckPrintf("0.10000000149011612", "{}", 0.1f);
	CheckPrintf("0.10000000149011612", "{}", (F64)0.1f);
	CheckPrintf("1.3563156426940112e-19", "{}", 1.35631564e-19f);

	// NaN
	// The standard allows implementation-specific suffixes following nan, for example as -nan formats as nan(ind) in MSVC.
	// These tests may need to be changed when porting to different platforms.
	constexpr F64 nan = std::numeric_limits<F64>::quiet_NaN();
	CheckPrintf("nan", "{}", nan);
	CheckPrintf("+nan", "{+}", nan);
	CheckPrintf("  +nan", "{+6}", nan);
	CheckPrintf("  +nan", "{+06}", nan);
	CheckPrintf("+nan  ", "{<+6}", nan);
	CheckPrintf("-nan", "{}", -nan);
	CheckPrintf("       -nan", "{+011}", -nan);
	CheckPrintf(" nan", "{ }", nan);
	CheckPrintf("nan    ", "{<7}", nan);
	CheckPrintf("    nan", "{7}", nan);

	// Inf
	constexpr F64 inf = std::numeric_limits<F64>::infinity();
	CheckPrintf("inf", "{}", inf);
	CheckPrintf("+inf", "{+}", inf);
	CheckPrintf("-inf", "{}", -inf);
	CheckPrintf("  +inf", "{+06}", inf);
	CheckPrintf("  -inf", "{+06}", -inf);
	CheckPrintf("+inf  ", "{<+6}", inf);
	CheckPrintf("  +inf", "{+6}", inf);
	CheckPrintf(" inf", "{ }", inf);
	CheckPrintf("inf    ", "{<7}", inf);
	CheckPrintf("    inf", "{7}", inf);

	// Char
	CheckPrintf("a", "{}", 'a');
	CheckPrintf("x", "{1}", 'x');
	CheckPrintf("  x", "{3}", 'x');
	CheckPrintf("\n", "{}", '\n');
	volatile char x = 'x';
	CheckPrintf("x", "{}", x);

	// Unsigned char
	CheckPrintf("42", "{}", static_cast<unsigned char>(42));
	CheckPrintf("42", "{}", static_cast<uint8_t>(42));

	// C string
	CheckPrintf("test", "{}", "test");
	char nonconst[] = "nonconst";
	CheckPrintf("nonconst", "{}", nonconst);

	// Pointer
	CheckPrintf("0x0000000000000000", "{}", static_cast<void*>(nullptr));
	CheckPrintf("0x0000000000001234", "{}", reinterpret_cast<void*>(0x1234));
	CheckPrintf("0xffffffffffffffff", "{}", reinterpret_cast<void*>(~uintptr_t()));
	CheckPrintf("0x0000000000000000", "{}", nullptr);
	
	CheckPrintf("1.234000:0042:+3.13:str:0x00000000000003e8:X%", "{0.6}:{04}:{+}:{}:{}:{}%", 1.234, 42, 3.13, "str", reinterpret_cast<void*>(1000), 'X');

	// Multibyte codepoint params
	// Note we support multibyte UTF-8 in the args, not in the actual format string
	CheckPrintf("hello abcʦϧ턖릇块𒃶𒅋🀅🚉🤸, nice to meet you", "hello {}, nice to meet you", "abcʦϧ턖릇块𒃶𒅋🀅🚉🤸");

	// Misc tests from examples and such
	CheckPrintf("First, thou shalt count to three", "First, thou shalt count to {}", "three");
	CheckPrintf("Bring me a shrubbery", "Bring me a {}", "shrubbery");
	CheckPrintf("From 1 to 3", "From {} to {}", 1, 3);
	CheckPrintf("-1.20", "{03.2f}", -1.2);
	CheckPrintf("a, b, c", "{}, {}, {}", 'a', 'b', 'c');
	CheckPrintf("abracadabra", "{}{}", "abra", "cadabra");
	CheckPrintf("left aligned                  ", "{<30}", "left aligned");
	CheckPrintf("                 right aligned", "{30}", "right aligned");
	CheckPrintf("+3.14; -3.14", "{+f}; {+f}", 3.14, -3.14);
	CheckPrintf(" 3.14; -3.14", "{ f}; { f}", 3.14, -3.14);
	CheckPrintf("bin: 101010; dec: 42; hex: 2a", "bin: {b}; dec: {}; hex: {x}", 42u, 42, 42u);
	CheckPrintf("The answer is 42", "The answer is {}", 42);
	CheckPrintf("1^2<3>", "{}^{}<{}>", 1, 2, 3);	// ParseSpec not called on empty placeholders
}
/**/