#include "JC/Fmt.h"

#include "JC/Array.h"
#include "JC/UnitTest.h"
#include "dragonbox/dragonbox.h"
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u32 Flag_Left  = 1 << 0;
static constexpr u32 Flag_Plus  = 1 << 1;
static constexpr u32 Flag_Space = 1 << 2;
static constexpr u32 Flag_Zero  = 1 << 3;
static constexpr u32 Flag_Bin   = 1 << 4;
static constexpr u32 Flag_Hex   = 1 << 5;
static constexpr u32 Flag_Fix   = 1 << 6;
static constexpr u32 Flag_Sci   = 1 << 7;

//--------------------------------------------------------------------------------------------------	

struct FixedOut {
	char* begin;
	char* end;

	void Add(char c) {
		if (begin < end) {
			*begin++ = c;
		}
	}
	void Add(const char* vals, u64 valsLen) {
		const u64 len = (u64)(end - begin);
		if (valsLen > len) {
			valsLen = len;
		}
		memcpy(begin, vals, valsLen);
		begin += valsLen;
	}

	void Add(const char* valsBegin, const char* valsEnd) {
		u64 valsLen = Min((u64)(valsEnd - valsBegin), (u64)(end - begin));
		memcpy(begin, valsBegin, valsLen);
		begin += valsLen;
	}

	void Fill(char c, u64 count) {
		const u64 len = (u64)(end - begin);
		if (count > len) {
			count = len;
		}
		memset(begin, c, count);
		begin += count;
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

s8 U64ToDigits(u64 u, char* out, u32 outLen) {
	// TODO: test and profile splitting into two 32-bit ints to avoid 64-bit divides
	char* const end = out + outLen;
	char* iter = end;
	while (u >= 100) {
		iter -= 2;
		*(u16*)iter = *(u16*)&Tens[(u % 100) * 2];
		u /= 100;
	}
	if (u >= 10) {
		iter -= 2;
		*(u16*)iter = *(u16*)&Tens[u * 2];
	}
	else
	{
		*--iter = '0' + (char)u;
	}
	return s8(iter, end);
}

s8 U64ToHexits(u64 u, char* out, u32 outLen) {
	char* const end = out + outLen;
	char* iter = end;
	constexpr const char* hexits = "0123456789abcdef";
	do {
		*--iter = hexits[u & 0xf];
		u >>= 4;
	} while (u > 0);
	return s8(iter, (u64)(end - iter));
}

s8 U64ToBits(u64 u, char* out, u32 outLen) {
	char* const end = out + outLen;
	char* iter = end;
	constexpr const char* bits = "01";
	do {
		*--iter = bits[u & 0x1];
		u >>= 1;
	} while (u > 0);
	return s8(iter, (u64)(end - iter));
}

//--------------------------------------------------------------------------------------------------	

// This function assumes there's a space right before ptr to hold the possible rounding character
bool Round(char* ptr, u64 len, u64 round) {
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
void WriteF64(Out* out, f64 f, u32 flags, u32 width, u32 prec) {
	char sign;
	if (signbit(f))              { sign = '-'; f = -f; }
	else if (flags & Flag_Plus)  { sign = '+'; }
	else if (flags & Flag_Space) { sign = ' '; }
	else                         { sign = '\0'; }

	if (const int fpc = fpclassify(f); fpc == FP_INFINITE || fpc == FP_NAN) {
		const s8 str = (fpc == FP_INFINITE) ? "inf" : "nan";
		const u32 len = (sign ? 1 : 0) + 3;
		const u32 pad = (width > len) ? width - len: 0;
		if (!(flags & Flag_Left)) { out->Fill(' ', pad); }
		if (sign) { out->Add(sign); }
		out->Add(str.data, str.len);
		if (flags & Flag_Left) { out->Fill(' ', pad); }
		return;
	}

	constexpr u32 MaxSigDigs = 17;
	char sigBuf[MaxSigDigs + 1] = "";
	s8   sigStr;
	u32  intDigs        = 0;
	u32  intTrailZeros  = 0;
	u32  fracLeadZeros  = 0;
	u32  fracDigs       = 0;
	u32  fracTrailZeros = 0;
	i32  exp            = 0;
	char dispExpSign    = '\0';
	i32  dispExp        = 0;

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

	const bool sci = (flags & Flag_Sci) || (!(flags & Flag_Fix) && (exp >= 6 || ((i32)sigStr.len + exp) <= -4));
	if (sci) {
		intDigs = 1u;
		fracLeadZeros = (sigStr.len == 1u) ? 1u : 0u;
		fracDigs = (u32)(sigStr.len - 1u);
		dispExp = exp + (i32)sigStr.len - 1;
		if (dispExp < 0) {
			dispExp = -dispExp;
			dispExpSign = '-';
		}

	// 12345e3 -> 12345000.0
	} else if (exp >= 0) {
		intDigs = (u32)sigStr.len;
		intTrailZeros = (u32)exp;
		fracTrailZeros = (prec == 0) ? 1 : prec;

	// 12345e-2 -> 123.45
	} else if (sigStr.len > (u64)(-exp)) {
		intDigs = (u32)((i32)sigStr.len + exp);
		fracDigs = (u32)(-exp);

	// 12345e-9 -> 0.000012345
	// 99999e-5 -> 0.99999
	} else {	// sigDigs <= -exp
		intDigs = 0;
		intTrailZeros = 1;
		fracLeadZeros = (u32)(-exp) - (u32)sigStr.len;
		fracDigs = (u32)sigStr.len;
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

	u32 totalLen = (sign ? 1 : 0) + intDigs + intTrailZeros + 1 + fracLeadZeros + fracDigs + fracTrailZeros;
	if (sci) {
		totalLen += 1 + (dispExpSign ? 1 : 0);
		     if (exp <=   9) { totalLen += 1; }
		else if (exp <=  99) { totalLen += 2; }
		else if (exp <= 999) { totalLen += 3; }
		else                 { totalLen += 4; }
	}
	const u32 pad = (width > totalLen) ? width - totalLen : 0;

	u32 intLeadZeros = 0;
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
		i32 de = dispExp;	// local copy for mutation
		if (de >= 100) {
			const char* const ten = &Tens[(u64)(de / 100) * 2];
			if (de >= 10000) {
				out->Add(ten[0]);
			}
			out->Add(ten[1]);
			de %= 100;
		}
		const char* const ten = &Tens[(u64)de * 2];
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
void WriteStr(Out* out, s8 str, u32 flags, u32 width, u32 prec) {
	if (prec && str.len > prec) {
		str.len = prec;
	}
	const u32 pad = (width > (u32)str.len) ? width - (u32)str.len : 0;
	bool left = flags & Flag_Left;
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
void WriteI64(Out* out, i64 i, u32 flags, u32 width) {
	char sign;
	u32 totalLen = 0;
	     if (i < 0)              { i = -i; sign = '-'; totalLen = 1; }
	else if (flags & Flag_Plus)  {         sign = '+'; totalLen = 1; }
	else if (flags & Flag_Space) {         sign = ' '; totalLen = 1; }
	else                         {         sign = '\0'; }

	char buf[72];
	const s8 str = U64ToDigits((u64)i, buf, sizeof(buf));
	totalLen += (u32)str.len;
	const u32 pad = (width > totalLen) ? width - totalLen : 0;
	u32 zeros = 0;
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
void WriteU64(Out* out, u64 u, u32 flags, u32 width) {
	char buf[72];
	s8 str;
	     if (flags & Flag_Hex) { str = U64ToHexits(u, buf, sizeof(buf)); }
	else if (flags & Flag_Bin) { str = U64ToBits  (u, buf, sizeof(buf)); }
	else                       { str = U64ToDigits(u, buf, sizeof(buf)); }
	const u32 pad = (width > (u32)str.len) ? width - (u32)str.len : 0;
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
void WritePtr(Out* out, const void* p, u32 flags, u32 width) {
	char buf[16];
	char* bufIter = buf + sizeof(buf);
	u64 u = (u64)p;
	for (u32 i = 0; i < 16; i++) {
		*--bufIter= "0123456789abcdef"[u & 0xf];
		u >>= 4;
	}

	constexpr u32 totalLen = 18;
	const u32 pad = (width > totalLen) ? width - totalLen : 0;
	u32 zeros = 0;
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
void VFmtImpl(Out out, s8 fmt, VArgs args) {
	const char*       i       = fmt.data;
	const char* const end     = i + fmt.len;
	u32               nextArg = 0;

	for (;;) {
		u32 flags = 0;
		u32 width = 0;
		u32 prec = 0;
		ScanUntilPlaceholder:
		const char* text = i;
		for (;;) {
			if (i >= end) {
				out->Add(text, i);
				Assert(nextArg == args.len, "Too many args");
				return;
			} else if (*i == '{') {
				out->Add(text, i);
				i++;
				break;
			} else if (*i != '}') {
				i++;
			} else {
				i++;
				out->Add(text, i);
				Assert(i < end && *i == '}');
				i++;
				text = i;
			}
		}

		while (i < end) {
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
		while (i < end && *i >= '0' && *i <= '9') {
			width = width * 10 + (*i - '0');
			i++;
		}
		if (i < end && *i == '.') {
			i++;
			while (i < end && *i >= '0' && *i <= '9') {
				prec = prec * 10 + (*i - '0');
				i++;
			}
		}
		Assert(i < end);
		switch (*i) {
			case 'x': flags |= Flag_Hex; i++; break;
			case 'X': flags |= Flag_Hex; i++; break;
			case 'b': flags |= Flag_Bin; i++; break;
			case 'f': flags |= Flag_Fix; i++; break;
			case 'e': flags |= Flag_Sci; i++; break;
			default:                          break;
		}
		Assert(i < end);
		Assert(*i == '}');
		i++;

		DoArg:
		Assert(nextArg < args.len);
		const VArg* arg = &args.args[nextArg++];
		switch (arg->type)
		{
			case VArgType::Bool: WriteStr(out, arg->b ? "true" : "false",   flags, width, prec); break;
			case VArgType::Char: WriteStr(out, s8(&arg->c, 1),              flags, width, prec); break;
			case VArgType::I64:  WriteI64(out, arg->i,                      flags, width);       break;													   
			case VArgType::U64:  WriteU64(out, arg->u,                      flags, width);       break;													   
			case VArgType::F64:  WriteF64(out, arg->f,                      flags, width, prec); break;
			case VArgType::S8:   WriteStr(out, s8(arg->s.data, arg->s.len), flags, width, prec); break;
			case VArgType::Ptr:  WritePtr(out, arg->p,                      flags, width);       break;
			default: Panic("Unhandled arg type {}", arg->type);
		}
	}
}

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, s8 fmt, VArgs args) {
	FixedOut out = { .begin = outBegin, .end = outEnd };
	VFmtImpl(&out, fmt, args);
	return out.begin;
}

void VFmt(Array<char>* out, s8 fmt, VArgs args) {
	VFmtImpl(out, fmt, args);
}
	
s8 VFmt(Arena* arena, s8 fmt, VArgs args) {
	Array<char> out(arena);
	VFmtImpl(&out, fmt, args);
	return s8(out.data, out.len);
}
	
//--------------------------------------------------------------------------------------------------

UnitTest("Fmt") {
	#define CheckFmt(expect, fmt, ...) { CheckEq(expect, Fmt(temp, fmt, ##__VA_ARGS__)); }

	// Escape sequences
	CheckFmt("{", "{{");
	CheckFmt("{", "{{");
	CheckFmt("before {", "before {{");
	CheckFmt("{ after", "{{ after");
	CheckFmt("before { after", "before {{ after");
	CheckFmt("}", "}}");
	CheckFmt("before }", "before }}");
	CheckFmt("} after", "}} after");
	CheckFmt("before } after", "before }} after");
	CheckFmt("{}", "{{}}");
	CheckFmt("{42}", "{{{}}}", 42);

	// Basic args
	CheckFmt("abc", "{}{}{}", 'a', 'b', 'c');
	CheckFmt("a 1 b -5 c xyz", "a {} b {} c {}", 1, -5, "xyz");

	// Left align
	CheckFmt("42  ", "{<4}", 42);
	CheckFmt("42  ", "{<4x}", 0x42u);
	CheckFmt("-42  ", "{<5}", -42);
	CheckFmt("42   ", "{<5}", 42u);
	CheckFmt("-42  ", "{<5}", -42l);
	CheckFmt("42   ", "{<5}", 42ul);
	CheckFmt("-42  ", "{<5}", -42ll);
	CheckFmt("42   ", "{<5}", 42ull);
	CheckFmt("-42.0  ", "{<7}", -42.0);
	CheckFmt("true   ", "{<7}", true);
	CheckFmt("c    ", "{<5}", 'c');
	CheckFmt("123456789", "{<5}", "123456789");
	CheckFmt("abc  ", "{<5}", "abc");
	CheckFmt("0x12345678abcdef12  ", "{<20}", reinterpret_cast<void*>(0x12345678abcdef12));

	// Sign '+'
	CheckFmt("+42", "{+}", 42);
	CheckFmt("-42", "{+}", -42);
	CheckFmt("+42", "{+}", 42);
	CheckFmt("+42", "{+}", 42l);
	CheckFmt("+42", "{+}", 42ll);
	CheckFmt("+42.0", "{+}", 42.0);

	// Sign ' '
	CheckFmt(" 42", "{ }", 42);
	CheckFmt("-42", "{ }", -42);
	CheckFmt(" 42", "{ }", 42);
	CheckFmt(" 42", "{ }", 42l);
	CheckFmt(" 42", "{ }", 42ll);
	CheckFmt(" 42.0", "{ }", 42.0);
	CheckFmt("-42.0", "{ }", -42.0);

	// Zero-padding '0'
	CheckFmt("42", "{0}", 42);
	CheckFmt("-0042", "{05}", -42);
	CheckFmt("00042", "{05}", 42u);
	CheckFmt("-0042", "{05}", -42l);
	CheckFmt("00042", "{05}", 42ul);
	CheckFmt("-0042", "{05}", -42ll);
	CheckFmt("00042", "{05}", 42ull);
	CheckFmt("0000042.0", "{09}",  42.0);
	CheckFmt("-000042.0", "{09}", -42.0);

	// Width
	CheckFmt(" -42", "{4}", -42);
	CheckFmt("   42", "{5}", 42u);
	CheckFmt("   -42", "{6}", -42l);
	CheckFmt("     42", "{7}", 42ul);
	CheckFmt("   -42", "{6}", -42ll);
	CheckFmt("     42", "{7}", 42ull);
	CheckFmt("   -1.23", "{8}", -1.23);
	CheckFmt("  0x12345678abcdef12", "{20}", reinterpret_cast<void*>(0x12345678abcdef12));
	CheckFmt("       true", "{11}", true);
	CheckFmt("          x", "{11}", 'x');
	CheckFmt("         str", "{12}", "str");
	CheckFmt("abcdef", "{5}", "abcdef");
	CheckFmt("abcdef", "{4}", "abcdef");
	CheckFmt("0000.0", "{06.1}", 0.00884311);

	// Precision
	CheckFmt("1.23", "{.2}", 1.2345);
	CheckFmt("1.23e56", "{.2}", 1.234e56);
	CheckFmt("1.100", "{.3}", 1.1);
	CheckFmt("1.00e0", "{.2e}", 1.0);
	CheckFmt("000000.000e0", "{012.3e}", 0.0);
	CheckFmt("123.5", "{.1}", 123.456);
	CheckFmt("123.46", "{.2}", 123.456);
	CheckFmt("1.23", "{.000002}", 1.234);
	CheckFmt("1019666432.0", "{}", 1019666432.0f);
	CheckFmt("9.6e0", "{.1e}", 9.57);
	CheckFmt("1.00e-34", "{.2e}", 1e-34);

	// Bool
	CheckFmt("true", "{}", true);
	CheckFmt("false", "{}", false);
	CheckFmt("true ", "{<5}", true);
	CheckFmt(" false", "{6}", false);

	// Short
	CheckFmt("42", "{}", (short)42);
	CheckFmt("42", "{}", (unsigned short)42);

	// Binary
	CheckFmt("0", "{b}", 0u);
	CheckFmt("101010", "{b}", 42u);
	CheckFmt("11000000111001", "{b}", 12345u);
	CheckFmt("10010001101000101011001111000", "{b}", 0x12345678u);
	CheckFmt("10010000101010111100110111101111", "{b}", 0x90abcdefu);
	CheckFmt("11111111111111111111111111111111", "{b}", 0xffffffffu);

	// Decimal
	CheckFmt("0", "{}", 0);
	CheckFmt("42", "{}", 42);
	CheckFmt("42", "{}", 42u);
	CheckFmt("-42", "{}", -42);
	CheckFmt("12345", "{}", 12345);
	CheckFmt("67890", "{}", 67890);

	// TODO: INT_MIN, ULONG_MAX, etc for hex/bin/dec
	// TODO: check unknown types
	// TODO: test with maxint as the precision literal

	// Hex
	CheckFmt("0", "{x}", 0u);
	CheckFmt("42", "{x}", 0x42u);
	CheckFmt("12345678", "{x}", 0x12345678u);
	CheckFmt("90abcdef", "{x}", 0x90abcdefu);
	
	// Float
	CheckFmt("0.0", "{}", 0.0f);
	CheckFmt("392.5", "{}", 392.5f);

	// Double
	CheckFmt("0.0", "{}", 0.0);
	CheckFmt("392.65", "{}", 392.65);
	CheckFmt("3.9265e2", "{e}", 392.65);
	CheckFmt("4901400.0", "{}", 4.9014e6);
	CheckFmt("+00392.6500", "{+011.4}", 392.65);
	CheckFmt("9223372036854776000.0", "{}", 9223372036854775807.0);

	// Precision rounding
	CheckFmt("0.000", "{.3f}", 0.00049);
	CheckFmt("0.000", "{.3f}", 0.0005);
	CheckFmt("0.002", "{.3f}", 0.0015);
	CheckFmt("0.001", "{.3f}", 0.00149);
	CheckFmt("0.002", "{.3f}", 0.0015);
	CheckFmt("1.000", "{.3f}", 0.9999);
	CheckFmt("0.001", "{.3}", 0.00123);
	CheckFmt("0.1000000000000000", "{.16}", 0.1);
	CheckFmt("225.51575035152064000", "{.17f}", 225.51575035152064);
	CheckFmt("-761519619559038.2", "{.1f}", -761519619559038.2);
	CheckFmt("1.9156918820264798e-56", "{}", 1.9156918820264798e-56);
	CheckFmt("0.0000", "{.4f}", 7.2809479766055470e-15);
	CheckFmt("3788512123356.9854", "{f}", 3788512123356.985352);

	// Float formatting
	CheckFmt("0.001", "{}", 1e-3);
	CheckFmt("0.0001", "{}", 1e-4);
	CheckFmt("1.0e-5", "{}", 1e-5);
	CheckFmt("1.0e-6", "{}", 1e-6);
	CheckFmt("1.0e-7", "{}", 1e-7);
	CheckFmt("1.0e-8", "{}", 1e-8);
	CheckFmt("1.0e15", "{}", 1e15);
	CheckFmt("1.0e16", "{}", 1e16);
	CheckFmt("9.999e-5", "{}", 9.999e-5);
	CheckFmt("1.234e10", "{}", 1234e7);
	CheckFmt("12.34", "{}", 1234e-2);
	CheckFmt("0.001234", "{}", 1234e-6);
	CheckFmt("0.10000000149011612", "{}", 0.1f);
	CheckFmt("0.10000000149011612", "{}", (f64)0.1f);
	CheckFmt("1.3563156426940112e-19", "{}", 1.35631564e-19f);

	// NaN
	// The standard allows implementation-specific suffixes following nan, for example as -nan formats as nan(ind) in MSVC.
	// These tests may need to be changed when porting to different platforms.
	constexpr f64 nan = std::numeric_limits<f64>::quiet_NaN();
	CheckFmt("nan", "{}", nan);
	CheckFmt("+nan", "{+}", nan);
	CheckFmt("  +nan", "{+6}", nan);
	CheckFmt("  +nan", "{+06}", nan);
	CheckFmt("+nan  ", "{<+6}", nan);
	CheckFmt("-nan", "{}", -nan);
	CheckFmt("       -nan", "{+011}", -nan);
	CheckFmt(" nan", "{ }", nan);
	CheckFmt("nan    ", "{<7}", nan);
	CheckFmt("    nan", "{7}", nan);

	// Inf
	constexpr f64 inf = std::numeric_limits<f64>::infinity();
	CheckFmt("inf", "{}", inf);
	CheckFmt("+inf", "{+}", inf);
	CheckFmt("-inf", "{}", -inf);
	CheckFmt("  +inf", "{+06}", inf);
	CheckFmt("  -inf", "{+06}", -inf);
	CheckFmt("+inf  ", "{<+6}", inf);
	CheckFmt("  +inf", "{+6}", inf);
	CheckFmt(" inf", "{ }", inf);
	CheckFmt("inf    ", "{<7}", inf);
	CheckFmt("    inf", "{7}", inf);

	// Char
	CheckFmt("a", "{}", 'a');
	CheckFmt("x", "{1}", 'x');
	CheckFmt("  x", "{3}", 'x');
	CheckFmt("\n", "{}", '\n');
	volatile char x = 'x';
	CheckFmt("x", "{}", x);

	// Unsigned char
	CheckFmt("42", "{}", static_cast<unsigned char>(42));
	CheckFmt("42", "{}", static_cast<uint8_t>(42));

	// C string
	CheckFmt("test", "{}", "test");
	char nonconst[] = "nonconst";
	CheckFmt("nonconst", "{}", nonconst);

	// Pointer
	CheckFmt("0x0000000000000000", "{}", static_cast<void*>(nullptr));
	CheckFmt("0x0000000000001234", "{}", reinterpret_cast<void*>(0x1234));
	CheckFmt("0xffffffffffffffff", "{}", reinterpret_cast<void*>(~uintptr_t()));
	CheckFmt("0x0000000000000000", "{}", nullptr);
	
	CheckFmt("1.234000:0042:+3.13:str:0x00000000000003e8:X%", "{0.6}:{04}:{+}:{}:{}:{}%", 1.234, 42, 3.13, "str", reinterpret_cast<void*>(1000), 'X');

	// Multibyte codepoint params
	// Note we support multibyte UTF-8 in the args, not in the actual format string
	CheckFmt("hello abcʦϧ턖릇块𒃶𒅋🀅🚉🤸, nice to meet you", "hello {}, nice to meet you", "abcʦϧ턖릇块𒃶𒅋🀅🚉🤸");

	// Misc tests from examples and such
	CheckFmt("First, thou shalt count to three", "First, thou shalt count to {}", "three");
	CheckFmt("Bring me a shrubbery", "Bring me a {}", "shrubbery");
	CheckFmt("From 1 to 3", "From {} to {}", 1, 3);
	CheckFmt("-1.20", "{03.2f}", -1.2);
	CheckFmt("a, b, c", "{}, {}, {}", 'a', 'b', 'c');
	CheckFmt("abracadabra", "{}{}", "abra", "cadabra");
	CheckFmt("left aligned                  ", "{<30}", "left aligned");
	CheckFmt("                 right aligned", "{30}", "right aligned");
	CheckFmt("+3.14; -3.14", "{+f}; {+f}", 3.14, -3.14);
	CheckFmt(" 3.14; -3.14", "{ f}; { f}", 3.14, -3.14);
	CheckFmt("bin: 101010; dec: 42; hex: 2a", "bin: {b}; dec: {}; hex: {x}", 42u, 42, 42u);
	CheckFmt("The answer is 42", "The answer is {}", 42);
	CheckFmt("1^2<3>", "{}^{}<{}>", 1, 2, 3);	// ParseSpec not called on empty placeholders
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC