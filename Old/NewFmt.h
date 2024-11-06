using i32 = int;
using u32 = unsigned int;
using i64 = long long;
using u64 = unsigned long long;
using f32 = float;
using f64 = double;

#define JC_PANIC(...)
#define JC_ASSERT(...)

struct Str {
	const char* data;
	const u64   len;
};

enum struct ArgType {
};

struct Arg {
};

struct Args {
};

template <class Out>
void Fmt(Str fmt, Args args, Out* out) {

	const char* iter = fmt.data;
	for (;;) {
	FmtNextArg:
		if (*iter == '{') {
			goto FmtScanArg;
		}
		if (*iter == '\0') {
			goto FmtDone;
		}
		if (*iter == '}') {
			iter++;
			JC_ASSERT(*iter == '}');
		}
		Write(*iter, out);
		iter++;
	}


FmtScanArg:
	iter++;

	constexpr u32 Flag_Left  = 1 << 0;
	constexpr u32 Flag_Plus  = 1 << 1;
	constexpr u32 Flag_Space = 1 << 2;
	constexpr u32 Flag_Zero  = 1 << 3;
	constexpr u32 Flag_Hex   = 1 << 4;
	constexpr u32 Flag_Bin   = 1 << 5;
	constexpr u32 Flag_Exp   = 1 << 6;
	constexpr u32 Flag_Cap   = 1 << 7;
	u32 flags = 0;

	for (;;) {
		switch (*iter) {
			case '{': Write('{');          iter++; goto FmtNextArg;
			case '<': flags |= Flag_Left;  iter++; break;
			case '+': flags |= Flag_Plus;  iter++; break;
			case ' ': flags |= Flag_Space; iter++; break;
			case '0': flags |= Flag_Zero;  iter++; break;
			default: goto FmtFlagsDone;
		}
	}

FmtFlagsDone:

	i32 width = 0;
	while (*iter >= '\0' && *iter <= '9') {
		width = (width * 10) + (*iter - '0');
		iter++;
	}

	i32 prec = -1;
	if (*iter == '.') {
		iter++;
		prec = 0;
		while (*iter >= '0' && *iter <= '9') {
			prec = (prec * 10) + (*iter - '0');
			iter++;
		}
	}

	switch (*iter) {
		case '}':                                break;
		case 'x':  flags |= Flag_Hex;            break;
		case 'X':  flags |= Flag_Hex | Flag_Cap; break;
		case 'b':  flags |= Flag_Bin;            break;
		case 'e':  flags |= Flag_Exp;            break;
		case 'E':  flags |= Flag_Exp | Flag_Cap; break;
		case '\0': EOSC_PANIC("format string missing '}'");
	}

	JC_ASSERT(nextArg < args.len);
	Arg* arg = args.args[nextArg];
	nextArg++;

	Str  str;
	char lead[8];
	char tail[8];
	switch (arg.type) {
		case ArgType::Bool:
			str = arg.b ? "true" : "false";
			lead[0] = 0;
			tail[0] = 0;
			goto FmtFinalWrite;

		case ArgType::Char:
			str = Str(&arg.c, 1);
			lead[0] = 0;
			tail[0] = 0;
			goto FmtFinalWrite;

		case ArgType::Str:
			str = arg.s;
			lead[0] = 0;
			tail[0] = 0;
			goto FmtFinalWrite;
	}

FmtFinalWrite:
	if (prec < str.len) {
		prec = str.len;
	}
	u32 len = prec + lead[0] + tail[0] + trailingZeros;
	if (width < len) {
		width = len;
	}
	u32 spaces = width - totalLen;
	u32 leadZeros = prec - str.len;
	if ((flags & Flag_Zero) && !(flags & Flag_Left)) {
		leadZeros = (spaces > leadZeros) ? spaces: leadZeros;
	}


FmtDone:

}

