#include "JC/Fmt.h"

#include "JC/Array.h"
#include "JC/Mem.h"
#include "JC/UnitTest.h"
#include "dragonbox/dragonbox.h"
#include <math.h>

namespace JC::Fmt {

//--------------------------------------------------------------------------------------------------

static constexpr U32 Flag_Left  = 1 << 0;
static constexpr U32 Flag_Plus  = 1 << 1;
static constexpr U32 Flag_Space = 1 << 2;
static constexpr U32 Flag_Zero  = 1 << 3;
static constexpr U32 Flag_Bin   = 1 << 4;
static constexpr U32 Flag_Hex   = 1 << 5;
static constexpr U32 Flag_Fix   = 1 << 6;
static constexpr U32 Flag_Sci   = 1 << 7;

//--------------------------------------------------------------------------------------------------	

struct FixedOut {
	char* begin;
	const char* end;

	void Add(char c) {
		if (begin < end) {
			*begin++ = c;
		}
	}
	void Add(const char* vals, U64 valsLen) {
		const U64 len = Min(valsLen, (U64)(end - begin));
		memcpy(begin, vals, valsLen);
		begin += len;
	}

	void Add(const char* valsBegin, const char* valsEnd) {
		const U64 len = Min((U64)(valsEnd - valsBegin), (U64)(end - begin));
		memcpy(begin, valsBegin, len);
		begin += len;
	}

	void Fill(char c, U64 count) {
		const U64 len = Min(count, (U64)(end - begin));
		memset(begin, c, count);
		begin += len;
	}
};

//--------------------------------------------------------------------------------------------------

static constexpr const char* Tens = 
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

static Str U64ToDigits(U64 u, char* out, U32 outLen) {
	// TODO: test and profile splitting into two 32-bit ints to avoid 64-bit divides
	char* const end = out + outLen;
	char* iter = end;
	while (u >= 100) {
		iter -= 2;
		*(U16*)iter = *(U16*)&Tens[(u % 100) * 2];
		u /= 100;
	}
	if (u >= 10) {
		iter -= 2;
		*(U16*)iter = *(U16*)&Tens[u * 2];
	}
	else
	{
		*--iter = '0' + (char)u;
	}
	return Str(iter, end - iter);
}

static Str U64ToHexits(U64 u, char* out, U32 outLen) {
	char* const end = out + outLen;
	char* iter = end;
	constexpr const char* hexits = "0123456789abcdef";
	do {
		*--iter = hexits[u & 0xf];
		u >>= 4;
	} while (u > 0);
	return Str(iter, (U64)(end - iter));
}

static Str U64ToBits(U64 u, char* out, U32 outLen) {
	char* const end = out + outLen;
	char* iter = end;
	constexpr const char* bits = "01";
	do {
		*--iter = bits[u & 0x1];
		u >>= 1;
	} while (u > 0);
	return Str(iter, (U64)(end - iter));
}

//--------------------------------------------------------------------------------------------------	

// This function assumes there's a space right before ptr to hold the possible rounding character
static Bool Round(char* ptr, U64 len, U64 round) {
	const char rem = ptr[round];
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

template <class Out>
static void WriteF64(Out* out, F64 f, U32 flags, U32 width, U32 prec) {
	char sign;
	if (signbit(f))              { sign = '-'; f = -f; }
	else if (flags & Flag_Plus)  { sign = '+'; }
	else if (flags & Flag_Space) { sign = ' '; }
	else                         { sign = '\0'; }

	if (const int fpc = fpclassify(f); fpc == FP_INFINITE || fpc == FP_NAN) {
		const Str str = (fpc == FP_INFINITE) ? "inf" : "nan";
		const U32 len = (sign ? 1 : 0) + 3;
		const U32 pad = (width > len) ? width - len: 0;
		if (!(flags & Flag_Left)) { out->Fill(' ', pad); }
		if (sign) { out->Add(sign); }
		out->Add(str.data, str.len);
		if (flags & Flag_Left) { out->Fill(' ', pad); }
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
		sigStr = U64ToDigits(db.significand, sigBuf + 1, sizeof(sigBuf) - 1);
		exp = db.exponent;
	}

	const Bool sci = (flags & Flag_Sci) || (!(flags & Flag_Fix) && (exp >= 6 || ((I32)sigStr.len + exp) <= -4));
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
			if (Round((char*)sigStr.data, sigStr.len, intDigs + fracDigs)) {
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
	if (!(flags & Flag_Left)) {
		if (flags & Flag_Zero) {
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
		out->Add('e');
		if (dispExpSign) {
			out->Add('-');
		}
		I32 de = dispExp;	// local copy for mutation
		if (de >= 100) {
			const char* const ten = &Tens[(U64)(de / 100) * 2];
			if (de >= 10000) {
				out->Add(ten[0]);
			}
			out->Add(ten[1]);
			de %= 100;
		}
		const char* const ten = &Tens[(U64)de * 2];
		if (dispExp >= 10) {
			out->Add(ten[0]);
		}
		out->Add(ten[1]);
	}
	if (flags & Flag_Left) {
		out->Fill(' ', pad);
	}
}

//--------------------------------------------------------------------------------------------------

template <class Out>
static void WriteStr(Out* out, Str str, U32 flags, U32 width, U32 prec) {
	if (prec && str.len > prec) {
		str.len = prec;
	}
	const U32 pad = (width > (U32)str.len) ? width - (U32)str.len : 0;
	Bool left = flags & Flag_Left;
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
static void WriteI64(Out* out, I64 i, U32 flags, U32 width) {
	char sign;
	U32 totalLen = 0;
	     if (i < 0)              { i = -i; sign = '-'; totalLen = 1; }
	else if (flags & Flag_Plus)  {         sign = '+'; totalLen = 1; }
	else if (flags & Flag_Space) {         sign = ' '; totalLen = 1; }
	else                         {         sign = '\0'; }

	char buf[72];
	const Str str = U64ToDigits((U64)i, buf, sizeof(buf));
	totalLen += (U32)str.len;
	const U32 pad = (width > totalLen) ? width - totalLen : 0;
	U32 zeros = 0;
	if (!(flags & Flag_Left)) {
		if (flags & Flag_Zero) { zeros = pad; }
		else { out->Fill(' ', pad); }
	}
	if (sign) { out->Add(sign); }
	out->Fill('0', zeros);
	out->Add(str.data, str.len);
	if (flags & Flag_Left) { out->Fill(' ', pad); }
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
static void WriteU64(Out* out, U64 u, U32 flags, U32 width) {
	char buf[72];
	Str str;
	     if (flags & Flag_Hex) { str = U64ToHexits(u, buf, sizeof(buf)); }
	else if (flags & Flag_Bin) { str = U64ToBits  (u, buf, sizeof(buf)); }
	else                       { str = U64ToDigits(u, buf, sizeof(buf)); }
	const U32 pad = (width > (U32)str.len) ? width - (U32)str.len : 0;
	if (!(flags & Flag_Left)) {
		out->Fill((flags & Flag_Zero) ? '0' : ' ', pad);
	}
	out->Add(str.data, str.len);
	if (flags & Flag_Left) {
		out->Fill(' ', pad);
	}
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
static void WritePtr(Out* out, const void* p, U32 flags, U32 width) {
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
	if (!(flags & Flag_Left)) {
		if (flags & Flag_Zero) { zeros = pad; }
		else { out->Fill(' ', pad); }
	}
	out->Add("0x", 2);
	out->Fill('0', zeros);
	out->Add(buf, 16);
	if (flags & Flag_Left) { out->Fill(' ', pad); }
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
static void VPrintfImpl(Out out, const char* fmt, Span<const Arg> args) {
	const char* i = fmt;
	U32 nextArg = 0;

	for (;;) {
		U32 flags = 0;
		U32 width = 0;
		U32 prec = 0;
		ScanUntilPlaceholder:
		const char* text = i;
		for (;;) {
			if (!*i) {
				out->Add(text, i);
				JC_ASSERT(nextArg == args.len, "Too many args");
				return;
			} else if (*i == '{') {
				out->Add(text, i);
				i++;
				break;
			} else if (*i == '}') {
				i++;
				out->Add(text, i);
				JC_ASSERT(*i == '}');
				i++;
				text = i;
			} else {
				i++;
			}
		}

		for (;;) {
			switch (*i) {
				case '}':                      i++; goto DoArg;
				case '{': out->Add('{');       i++; goto ScanUntilPlaceholder;
				case '<': flags |= Flag_Left;  i++; break;
				case '+': flags |= Flag_Plus;  i++; break;
				case ' ': flags |= Flag_Space; i++; break;
				case '0': flags |= Flag_Zero;  i++; goto ScanWidthPrec;
				default:                            goto ScanWidthPrec;
			}
		}
		ScanWidthPrec:
		while (*i >= '0' && *i <= '9') {
			width = width * 10 + (*i - '0');
			i++;
		}
		if (*i == '.') {
			i++;
			while (*i >= '0' && *i <= '9') {
				prec = prec * 10 + (*i - '0');
				i++;
			}
		}
		switch (*i) {
			case 'x': flags |= Flag_Hex; i++; break;
			case 'X': flags |= Flag_Hex; i++; break;
			case 'b': flags |= Flag_Bin; i++; break;
			case 'f': flags |= Flag_Fix; i++; break;
			case 'e': flags |= Flag_Sci; i++; break;
			default:                          break;
		}
		JC_ASSERT(*i == '}');
		i++;

		DoArg:
		JC_ASSERT(nextArg < args.len);
		const Arg* arg = &args[nextArg++];
		switch (arg->type)
		{
			case ArgType::Bool: WriteStr(out, arg->b ? "true" : "false",    flags, width, prec); break;
			case ArgType::Char: WriteStr(out, Str(&arg->c, 1),              flags, width, prec); break;
			case ArgType::I64:  WriteI64(out, arg->i,                       flags, width);       break;													   
			case ArgType::U64:  WriteU64(out, arg->u,                       flags, width);       break;													   
			case ArgType::F64:  WriteF64(out, arg->f,                       flags, width, prec); break;
			case ArgType::Str:  WriteStr(out, Str(arg->s.data, arg->s.len), flags, width, prec); break;
			case ArgType::Ptr:  WritePtr(out, arg->p,                       flags, width);       break;
			default: JC_PANIC("Unhandled ArgType {}", arg->type);
		}
	}
}

//--------------------------------------------------------------------------------------------------

char* VPrintf(char* outBegin, const char* outEnd, const char* fmt, Span<const Arg> args) {
	FixedOut out = { .begin = outBegin, .end = outEnd };
	VPrintfImpl(&out, fmt, args);
	return out.begin;
}

void VPrintf(Array<char>* out, const char* fmt, Span<const Arg> args) {
	VPrintfImpl(out, fmt, args);
}
	
Str VPrintf(Mem::Allocator* allocator, const char* fmt, Span<const Arg> args) {
	Array<char> out(allocator);
	VPrintfImpl(&out, fmt, args);
	return Str(out.data, out.len);
}

//--------------------------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Fmt