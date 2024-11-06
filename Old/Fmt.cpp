#include "JC/UnitTest.h"

#pragma warning (disable: 26450)	// arithmetic operator causes overflow at compile time
#include "dragonbox/dragonbox.h"
#include <math.h>

namespace JC {

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
	return s8(iter, (u64)(end - iter));
}
abcdefghijklmnopqrstuvwxyz
ABCDEFGHIJKLMNOPQRSTUVWXYZ
0123456789-=_+{}[]()\/,.<>!@#$%^&*~`
aJ
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

static constexpr u32 Flag_Left  = 1 << 0;
static constexpr u32 Flag_Plus  = 1 << 1;
static constexpr u32 Flag_Space = 1 << 2;
static constexpr u32 Flag_Zero  = 1 << 3;
static constexpr u32 Flag_Bin   = 1 << 4;
static constexpr u32 Flag_Hex   = 1 << 5;
static constexpr u32 Flag_Fix   = 1 << 6;
static constexpr u32 Flag_Exp   = 1 << 7;

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
		const u32 pad = (width > 3) ? width - 3: 0;
		if (!(flags & Flag_Left)) { out->Fill(' ', pad); }
		out->Add(str.data, str.len);
		if (flags & Flag_Left) { out->Fill(' ', width); }
		return;
	}

	constexpr u32 MaxSigDigs = 17;
	char sigBuf[MaxSigDigs + 1] = "";
	s8  sigStr;
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
		sigStr = U64ToDigits(db.significand, sigBuf + 1, sizeof(sigBuf - 1));
		exp = db.exponent;
	}

	const bool sci = (flags & Flag_Exp) || (!(flags & Flag_Fix) && (exp >= 6 || ((i32)sigStr.len + exp) <= -4));
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
	if (prec > 0 && str.len > prec) {
		str.len = prec;
	}
	const u32 pad = (width > (u32)str.len) ? width - (u32)str.len : 0;
	if (!(flags & Flag_Left)) {
		out->Fill(' ', pad);
	}
	out->Add(str.data, str.len);
	if (flags & Flag_Left) {
		out->Fill(' ', width);
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
	if (flags & Flag_Left) { out->Fill(' ', width); }
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
		out->Fill(' ', width);
	}
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
void WritePtr(Out* out, const void* p, u32 flags, u32 width) {
	char buf[16];
	char* bufIter = buf + sizeof(buf);
	u64 u = (u64)p;
	for (u32 i = 0; i < 16; i++) {
		*--bufIter= "123456789abcdef"[u & 0xf];
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
	if (flags & Flag_Left) { out->Fill(' ', width); }
}

//--------------------------------------------------------------------------------------------------	

template <class Out>
void VFmtImpl(Out out, s8 fmt, Args args) {
	const char* iter = fmt.data;
	const char* const end = iter + fmt.len;
	const char* text = iter;
	u32 nextArg = 0;
	for (;;) {
		Scan:
		while (iter < end) {
			char c = *iter++;
			if (c == '{') {
				out->Add(text, iter - 1);
				break;
			} else if (c == '}') {
				Assert(iter < end && *iter == '}');
				iter++;
				out->Add('}');
			}
		}

		u32 flags = 0;
		while (iter < end) {
			switch (*iter) {
				case '<': flags |= Flag_Left;  iter++; break;
				case '+': flags |= Flag_Plus;  iter++; break;
				case ' ': flags |= Flag_Space; iter++; break;
				case '0': flags |= Flag_Zero;  iter++; goto FlagsDone;
				case '{': out->Add('{');       iter++; goto Scan;
				default:                               break;
			}
		}

		FlagsDone:

		u32 width = 0;
		while (iter < end && *iter >= '0' && *iter <= '9') {
			width = width * 10 + (*iter - '0');
			iter++;
		}

		u32 prec = 0;
		if (iter < end && *iter == '.') {
			iter++;
			while (iter < end && *iter >= '0' && *iter <= '9') {
				prec = prec * 10 + (*iter - '0');
				iter++;
			}
		}

		if (iter >= end) {
			// TODO: error
			goto Done;
		}

		switch (*iter) {
			case 'x': flags |= Flag_Hex; iter++; break;
			case 'b': flags |= Flag_Bin; iter++; break;
			case 'f': flags |= Flag_Fix; iter++; break;
			case 'e': flags |= Flag_Exp; iter++; break;
			default:                             break;
		}

		if (iter >= end || *iter != '}') {
			// TODO: error
			goto Done;
		}
		iter++;

		if (nextArg >= args.len) {
			// TODO: error;
			goto Done;
		}
		const Arg* arg = &args.args[nextArg++];

		switch (arg->type)
		{
			case ArgType::Bool:
				WriteStr(out, arg->b ? "true" : "false", flags, width, prec);
				break;

			case ArgType::Char:
				WriteStr(out, s8(&arg->c, 1), flags, width, prec);
				break;

			case ArgType::I64:
				WriteI64(out, arg->i, flags, width);
				break;

			case ArgType::U64:
				WriteU64(out, arg->u, flags, width);
				break;

			case ArgType::F64:
				WriteF64(out, arg->f, flags, width, prec);
				break;

			case ArgType::S8:
				WriteStr(out, s8(arg->s.d, arg->s.l), flags, width, prec);
				break;

			case ArgType::Ptr:
				WritePtr(out, arg->p, flags, width);
				break;

			default:
				// TODO: error;
				break;
		}
	}

	Done:

	if (nextArg < args.len) {
		// TODO: error;
	}
}

//--------------------------------------------------------------------------------------------------

char* VFmt(char* outBegin, char* outEnd, s8 fmt, Args args) {
	FixedOut out = { .begin = outBegin, .end = outEnd };
	VFmtImpl(&out, fmt, args);
	return out.begin;
}

void VFmt(Array<char>* out, s8 fmt, Args args) {
	VFmtImpl(out, fmt, args);
}
	
s8 VFmt(Allocator* allocator, s8 fmt, Args args) {
	Array<char> out;
	out.Init(allocator);
	VFmtImpl(&out, fmt, args);
	return s8(out.data, out.len);
}
	
//--------------------------------------------------------------------------------------------------

UnitTest("Fmt") {
	VirtualMemApi* virtualMemApi = VirtualMemApi::Init();
	TempAllocatorApi* tempAllocatorApi = TempAllocatorApi::Init(virtualMemApi);
	Allocator* ta = tempAllocatorApi->Get();

	// Escape sequences
	Check_Eq(Fmt(ta, "{{"), "{");
	Check_Eq(Fmt(ta, "{{"), "{");
	Check_Eq(Fmt(ta, "before {{"), "before {");
	Check_Eq(Fmt(ta, "{{ after"), "{ after");
	Check_Eq(Fmt(ta, "before {{ after"), "before { after");
	Check_Eq(Fmt(ta, "}}"), "}");
	Check_Eq(Fmt(ta, "before }}"), "before }");
	Check_Eq(Fmt(ta, "}} after"), "} after");
	Check_Eq(Fmt(ta, "before }} after"), "before } after");
	Check_Eq(Fmt(ta, "{{}}"), "{}");
	Check_Eq(Fmt(ta, "{{{}}}", 42), "{42}");

	// Basic args
	Check_Eq(Fmt(ta, "{}{}{}", 'a', 'b', 'c'), "abc");
	Check_Eq(Fmt(ta, "a {} b {} c {}", 1, -5, "xyz"), "a 1 b -5 c xyz");

	// Left align
	Check_Eq(Fmt(ta, "{<4}", 42), "42  ");
	Check_Eq(Fmt(ta, "{<4x}", 0x42u), "42  ");
	Check_Eq(Fmt(ta, "{<5}", -42), "-42  ");
	Check_Eq(Fmt(ta, "{<5}", 42u), "42   ");
	Check_Eq(Fmt(ta, "{<5}", -42l), "-42  ");
	Check_Eq(Fmt(ta, "{<5}", 42ul), "42   ");
	Check_Eq(Fmt(ta, "{<5}", -42ll), "-42  ");
	Check_Eq(Fmt(ta, "{<5}", 42ull), "42   ");
	Check_Eq(Fmt(ta, "{<7}", -42.0), "-42.0  ");
	Check_Eq(Fmt(ta, "{<7}", true), "true   ");
	Check_Eq(Fmt(ta, "{<5}", 'c'), "c    ");
	Check_Eq(Fmt(ta, "{<5}", "123456789"), "123456789");
	Check_Eq(Fmt(ta, "{<5}", "abc"), "abc  ");
	Check_Eq(Fmt(ta, "{<20}", reinterpret_cast<void*>(0x12345678abcdef12)), "0x12345678abcdef12  ");

	// Right align
	Check_Eq(Fmt(ta, "{>4}", 42), "  42");
	Check_Eq(Fmt(ta, "{>4x}", 0x42u), "  42");
	Check_Eq(Fmt(ta, "{>5}", -42), "  -42");
	Check_Eq(Fmt(ta, "{>5}", 42u), "   42");
	Check_Eq(Fmt(ta, "{>5}", -42l), "  -42");
	Check_Eq(Fmt(ta, "{>5}", 42ul), "   42");
	Check_Eq(Fmt(ta, "{>5}", -42ll), "  -42");
	Check_Eq(Fmt(ta, "{>5}", 42ull), "   42");
	Check_Eq(Fmt(ta, "{>7}", -42.0), "  -42.0");
	Check_Eq(Fmt(ta, "{>7}", true), "   true");
	Check_Eq(Fmt(ta, "{>5}", 'c'), "    c");
	Check_Eq(Fmt(ta, "{>5}", "abc"), "  abc");
	Check_Eq(Fmt(ta, "{>20}", reinterpret_cast<void*>(0x12345678abcdef12)), "  0x12345678abcdef12");

	// Center align
	Check_Eq(Fmt(ta, "{^5}", 42), " 42  ");
	Check_Eq(Fmt(ta, "{^5x}", 0x42u), " 42  ");
	Check_Eq(Fmt(ta, "{^5}", -42), " -42 ");
	Check_Eq(Fmt(ta, "{^5}", 42u), " 42  ");
	Check_Eq(Fmt(ta, "{^5}", -42l), " -42 ");
	Check_Eq(Fmt(ta, "{^5}", 42ul), " 42  ");
	Check_Eq(Fmt(ta, "{^5}", -42ll), " -42 ");
	Check_Eq(Fmt(ta, "{^5}", 42ull), " 42  ");
	Check_Eq(Fmt(ta, "{^8}", -42.0), " -42.0  ");
	Check_Eq(Fmt(ta, "{^7}", true), " true  ");
	Check_Eq(Fmt(ta, "{^5}", 'c'), "  c  ");
	Check_Eq(Fmt(ta, "{^6}", "abc"), " abc  ");
	Check_Eq(Fmt(ta, "{^23}", reinterpret_cast<void*>(0x12345678abcdef12)), "  0x12345678abcdef12   ");

	// Sign '+'
	Check_Eq(Fmt(ta, "{+}", 42), "+42");
	Check_Eq(Fmt(ta, "{+}", -42), "-42");
	Check_Eq(Fmt(ta, "{+}", 42), "+42");
	Check_Eq(Fmt(ta, "{+}", 42l), "+42");
	Check_Eq(Fmt(ta, "{+}", 42ll), "+42");
	Check_Eq(Fmt(ta, "{+}", 42.0), "+42.0");

	// Sign ' '
	Check_Eq(Fmt(ta, "{ }", 42), " 42");
	Check_Eq(Fmt(ta, "{ }", -42), "-42");
	Check_Eq(Fmt(ta, "{ }", 42), " 42");
	Check_Eq(Fmt(ta, "{ }", 42l), " 42");
	Check_Eq(Fmt(ta, "{ }", 42ll), " 42");
	Check_Eq(Fmt(ta, "{ }", 42.0), " 42.0");
	Check_Eq(Fmt(ta, "{ }", -42.0), "-42.0");

	// Zero-padding '0'
	Check_Eq(Fmt(ta, "{0}", 42), "42");
	Check_Eq(Fmt(ta, "{05}", -42), "-0042");
	Check_Eq(Fmt(ta, "{05}", 42u), "00042");
	Check_Eq(Fmt(ta, "{05}", -42l), "-0042");
	Check_Eq(Fmt(ta, "{05}", 42ul), "00042");
	Check_Eq(Fmt(ta, "{05}", -42ll), "-0042");
	Check_Eq(Fmt(ta, "{05}", 42ull), "00042");
	Check_Eq(Fmt(ta, "{09}",  42.0), "0000042.0");
	Check_Eq(Fmt(ta, "{09}", -42.0), "-000042.0");

	// Width
	Check_Eq(Fmt(ta, "{4}", -42), " -42");
	Check_Eq(Fmt(ta, "{5}", 42u), "   42");
	Check_Eq(Fmt(ta, "{6}", -42l), "   -42");
	Check_Eq(Fmt(ta, "{7}", 42ul), "     42");
	Check_Eq(Fmt(ta, "{6}", -42ll), "   -42");
	Check_Eq(Fmt(ta, "{7}", 42ull), "     42");
	Check_Eq(Fmt(ta, "{8}", -1.23), "   -1.23");
	Check_Eq(Fmt(ta, "{20}", reinterpret_cast<void*>(0x12345678abcdef12)), "  0x12345678abcdef12");
	Check_Eq(Fmt(ta, "{11}", true), "true       ");
	Check_Eq(Fmt(ta, "{11}", 'x'), "x          ");
	Check_Eq(Fmt(ta, "{12}", "str"), "str         ");
	Check_Eq(Fmt(ta, "{06.1}", 0.00884311), "0000.0");

	// Precision
	Check_Eq(Fmt(ta, "{.2}", 1.2345), "1.23");
	Check_Eq(Fmt(ta, "{.2}", 1.234e56), "1.23e56");
	Check_Eq(Fmt(ta, "{.3}", 1.1), "1.100");
	Check_Eq(Fmt(ta, "{.2e}", 1.0), "1.00e0");
	Check_Eq(Fmt(ta, "{012.3e}", 0.0), "000000.000e0");
	Check_Eq(Fmt(ta, "{.1}", 123.456), "123.5");
	Check_Eq(Fmt(ta, "{.2}", 123.456), "123.46");
	Check_Eq(Fmt(ta, "{.000002}", 1.234), "1.23");
	Check_Eq(Fmt(ta, "{}", 1019666432.0f), "1019666432.0");
	Check_Eq(Fmt(ta, "{.1e}", 9.57), "9.6e0");
	Check_Eq(Fmt(ta, "{.2e}", 1e-34), "1.00e-34");

	// Bool
	Check_Eq(Fmt(ta, "{}", true), "true");
	Check_Eq(Fmt(ta, "{}", false), "false");
	Check_Eq(Fmt(ta, "{>5}", true), " true");
	Check_Eq(Fmt(ta, "{6}", false), "false ");

	// Short
	Check_Eq(Fmt(ta, "{}", (short)42), "42");
	Check_Eq(Fmt(ta, "{}", (unsigned short)42), "42");

	// Binary
	Check_Eq(Fmt(ta, "{b}", 0u), "0");
	Check_Eq(Fmt(ta, "{b}", 42u), "101010");
	Check_Eq(Fmt(ta, "{b}", 12345u), "11000000111001");
	Check_Eq(Fmt(ta, "{b}", 0x12345678u), "10010001101000101011001111000");
	Check_Eq(Fmt(ta, "{b}", 0x90abcdefu), "10010000101010111100110111101111");
	Check_Eq(Fmt(ta, "{b}", 0xffffffffu), "11111111111111111111111111111111");

	// Decimal
	Check_Eq(Fmt(ta, "{}", 0), "0");
	Check_Eq(Fmt(ta, "{}", 42), "42");
	Check_Eq(Fmt(ta, "{}", 42u), "42");
	Check_Eq(Fmt(ta, "{}", -42), "-42");
	Check_Eq(Fmt(ta, "{}", 12345), "12345");
	Check_Eq(Fmt(ta, "{}", 67890), "67890");

	// TODO: INT_MIN, ULONG_MAX, etc for hex/bin/dec
	// TODO: check unknown types
	// TODO: test with maxint as the precision literal

	// Hex
	Check_Eq(Fmt(ta, "{x}", 0u), "0");
	Check_Eq(Fmt(ta, "{x}", 0x42u), "42");
	Check_Eq(Fmt(ta, "{x}", 0x12345678u), "12345678");
	Check_Eq(Fmt(ta, "{x}", 0x90abcdefu), "90abcdef");
	
	// Float
	Check_Eq(Fmt(ta, "{}", 0.0f), "0.0");
	Check_Eq(Fmt(ta, "{}", 392.5f), "392.5");

	// Double
	Check_Eq(Fmt(ta, "{}", 0.0), "0.0");
	Check_Eq(Fmt(ta, "{}", 392.65), "392.65");
	Check_Eq(Fmt(ta, "{e}", 392.65), "3.9265e2");
	Check_Eq(Fmt(ta, "{}", 4.9014e6), "4901400.0");
	Check_Eq(Fmt(ta, "{+011.4}", 392.65), "+00392.6500");
	Check_Eq(Fmt(ta, "{}", 9223372036854775807.0), "9223372036854776000.0");

	// Precision rounding
	Check_Eq(Fmt(ta, "{.3f}", 0.00049), "0.000");
	Check_Eq(Fmt(ta, "{.3f}", 0.0005), "0.000");
	Check_Eq(Fmt(ta, "{.3f}", 0.0015), "0.002");
	Check_Eq(Fmt(ta, "{.3f}", 0.00149), "0.001");
	Check_Eq(Fmt(ta, "{.3f}", 0.0015), "0.002");
	Check_Eq(Fmt(ta, "{.3f}", 0.9999), "1.000");
	Check_Eq(Fmt(ta, "{.3}", 0.00123), "0.001");
	Check_Eq(Fmt(ta, "{.16}", 0.1), "0.1000000000000000");
	Check_Eq(Fmt(ta, "{.17f}", 225.51575035152064), "225.51575035152064000");
	Check_Eq(Fmt(ta, "{.1f}", -761519619559038.2), "-761519619559038.2");
	Check_Eq(Fmt(ta, "{}", 1.9156918820264798e-56), "1.9156918820264798e-56");
	Check_Eq(Fmt(ta, "{.4f}", 7.2809479766055470e-15), "0.0000");
	Check_Eq(Fmt(ta, "{f}", 3788512123356.985352), "3788512123356.9854");

	// Float formatting
	Check_Eq(Fmt(ta, "{}", 1e-3), "0.001");
	Check_Eq(Fmt(ta, "{}", 1e-4), "0.0001");
	Check_Eq(Fmt(ta, "{}", 1e-5), "1.0e-5");
	Check_Eq(Fmt(ta, "{}", 1e-6), "1.0e-6");
	Check_Eq(Fmt(ta, "{}", 1e-7), "1.0e-7");
	Check_Eq(Fmt(ta, "{}", 1e-8), "1.0e-8");
	Check_Eq(Fmt(ta, "{}", 1e15), "1.0e15");
	Check_Eq(Fmt(ta, "{}", 1e16), "1.0e16");
	Check_Eq(Fmt(ta, "{}", 9.999e-5), "9.999e-5");
	Check_Eq(Fmt(ta, "{}", 1234e7), "1.234e10");
	Check_Eq(Fmt(ta, "{}", 1234e-2), "12.34");
	Check_Eq(Fmt(ta, "{}", 1234e-6), "0.001234");
	Check_Eq(Fmt(ta, "{}", 0.1f), "0.10000000149011612");
	Check_Eq(Fmt(ta, "{}", (double)0.1f), "0.10000000149011612");
	Check_Eq(Fmt(ta, "{}", 1.35631564e-19f), "1.3563156426940112e-19");

	// NaN
	// The standard allows implementation-specific suffixes following nan, for example as -nan formats as nan(ind) in MSVC.
	// These tests may need to be changed when porting to different platforms.
	constexpr double nan = std::numeric_limits<double>::quiet_NaN();
	Check_Eq(Fmt(ta, "{}", nan), "nan");
	Check_Eq(Fmt(ta, "{+}", nan), "+nan");
	Check_Eq(Fmt(ta, "{+06}", nan), "  +nan");
	Check_Eq(Fmt(ta, "{<+6}", nan), "+nan  ");
	Check_Eq(Fmt(ta, "{^+6}", nan), " +nan ");
	Check_Eq(Fmt(ta, "{>+6}", nan), "  +nan");
	Check_Eq(Fmt(ta, "{}", -nan), "-nan");
	Check_Eq(Fmt(ta, "{+011}", -nan), "       -nan");
	Check_Eq(Fmt(ta, "{ }", nan), " nan");
	Check_Eq(Fmt(ta, "{<7}", nan), "nan    ");
	Check_Eq(Fmt(ta, "{^7}", nan), "  nan  ");
	Check_Eq(Fmt(ta, "{>7}", nan), "    nan");

	// Inf
	constexpr double inf = std::numeric_limits<double>::infinity();
	Check_Eq(Fmt(ta, "{}", inf), "inf");
	Check_Eq(Fmt(ta, "{+}", inf), "+inf");
	Check_Eq(Fmt(ta, "{}", -inf), "-inf");
	Check_Eq(Fmt(ta, "{+06}", inf), "  +inf");
	Check_Eq(Fmt(ta, "{+06}", -inf), "  -inf");
	Check_Eq(Fmt(ta, "{<+6}", inf), "+inf  ");
	Check_Eq(Fmt(ta, "{^+6}", inf), " +inf ");
	Check_Eq(Fmt(ta, "{>+6}", inf), "  +inf");
	Check_Eq(Fmt(ta, "{ }", inf), " inf");
	Check_Eq(Fmt(ta, "{<7}", inf), "inf    ");
	Check_Eq(Fmt(ta, "{^7}", inf), "  inf  ");
	Check_Eq(Fmt(ta, "{>7}", inf), "    inf");

	// Char
	Check_Eq(Fmt(ta, "{}", 'a'), "a");
	Check_Eq(Fmt(ta, "{1}", 'x'), "x");
	Check_Eq(Fmt(ta, "{3}", 'x'), "x  ");
	Check_Eq(Fmt(ta, "{}", '\n'), "\n");
	volatile char x = 'x';
	Check_Eq(Fmt(ta, "{}", x), "x");

	// Unsigned char
	Check_Eq(Fmt(ta, "{}", static_cast<unsigned char>(42)), "42");
	Check_Eq(Fmt(ta, "{}", static_cast<uint8_t>(42)), "42");

	// C string
	Check_Eq(Fmt(ta, "{}", "test"), "test");
	char nonconst[] = "nonconst";
	Check_Eq(Fmt(ta, "{}", nonconst), "nonconst");

	// Pointer
	Check_Eq(Fmt(ta, "{}", static_cast<void*>(nullptr)), "0x0000000000000000");
	Check_Eq(Fmt(ta, "{}", reinterpret_cast<void*>(0x1234)), "0x0000000000001234");
	Check_Eq(Fmt(ta, "{}", reinterpret_cast<void*>(~uintptr_t())), "0xffffffffffffffff");
	Check_Eq(Fmt(ta, "{}", nullptr), "0x0000000000000000");
	
	Check_Eq(Fmt(ta, "{0.6}:{04}:{+}:{}:{}:{}%", 1.234, 42, 3.13, "str", reinterpret_cast<void*>(1000), 'X'), "1.234000:0042:+3.13:str:0x00000000000003e8:X%");

	// Multibyte codepoint params
	// Note we support multibyte UTF-8 in the args, not in the actual format string
	Check_Eq(Fmt(ta, "hello {}, nice to meet you", "abcʦϧ턖릇块⃶⅋露"), "hello abcʦϧ턖릇块⃶⅋露, nice to meet you");

	// Misc tests from examples and such
	Check_Eq(Fmt(ta, "First, thou shalt count to {}", "three"), "First, thou shalt count to three");
	Check_Eq(Fmt(ta, "Bring me a {}", "shrubbery"), "Bring me a shrubbery");
	Check_Eq(Fmt(ta, "From {} to {}", 1, 3), "From 1 to 3");
	Check_Eq(Fmt(ta, "{03.2f}", -1.2), "-1.20");
	Check_Eq(Fmt(ta, "{}, {}, {}", 'a', 'b', 'c'), "a, b, c");
	Check_Eq(Fmt(ta, "{}{}", "abra", "cadabra"), "abracadabra");
	Check_Eq(Fmt(ta, "{<30}", "left aligned"), "left aligned                  ");
	Check_Eq(Fmt(ta, "{>30}", "right aligned"), "                 right aligned");
	Check_Eq(Fmt(ta, "{^30}", "centered"), "           centered           ");
	Check_Eq(Fmt(ta, "{+f}; {+f}", 3.14, -3.14), "+3.14; -3.14");
	Check_Eq(Fmt(ta, "{ f}; { f}", 3.14, -3.14), " 3.14; -3.14");
	Check_Eq(Fmt(ta, "bin: {b}; dec: {}; hex: {x}", 42u, 42, 42u), "bin: 101010; dec: 42; hex: 2a");
	Check_Eq(Fmt(ta, "The answer is {}", 42), "The answer is 42");
	Check_Eq(Fmt(ta, "{}^{}<{}>", 1, 2, 3), "1^2<3>");	// ParseSpec not called on empty placeholders
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC