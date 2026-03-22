// TODO: make this match real json5: https://spec.json5.org/


#include "JC/Json.h"

#include "JC/Array.h"
#include "JC/StrDb.h"
#include "JC/UnitTest.h"
#include <math.h>

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, MissingClosingQuote);
DefErr(Json, Unexpected);
DefErr(Json, MissingMember);
DefErr(Json, UnknownMember);
DefErr(Json, BadEscChar);
DefErr(Json, BadName);
DefErr(Json, BadBool);
DefErr(Json, BadInt);
DefErr(Json, BadFloat);
DefErr(Json, BadSci);
DefErr(Json, Eof);
DefErr(Json, ArrayMaxLen);

//--------------------------------------------------------------------------------------------------

enum struct CharType : U8 {
	Other = 0,
	Space,
	Operator,
	Quote,
	Underscore,
	Alpha,
	Digit,
};

struct CharTable {
	CharType table[256];

	constexpr CharTable() {
		for (U32 i = 0; i < 256; i++) { table[i] = CharType::Other; }
		table[' ' ] = CharType::Space;
		table['\t'] = CharType::Space;
		table['\r'] = CharType::Space;
		table['\n'] = CharType::Space;
		table['{' ] = CharType::Operator;
		table['}' ] = CharType::Operator;
		table['[' ] = CharType::Operator;
		table[']' ] = CharType::Operator;
		table[':' ] = CharType::Operator;
		table[',' ] = CharType::Operator;
		table['"' ] = CharType::Quote;
		table['_' ] = CharType::Underscore;
		for (char c = 'a'; c <= 'z'; c++) { table[c] = CharType::Alpha; }
		for (char c = 'A'; c <= 'Z'; c++) { table[c] = CharType::Alpha; }
		for (char c = '0'; c <= '9'; c++) { table[c] = CharType::Digit; }
	}
};

static constexpr CharTable charTable;

static constexpr bool IsName(char c) {
	CharType charType = charTable.table[c];
	return charType == CharType::Underscore || charType == CharType::Alpha || charType == CharType::Digit;
}

static constexpr bool IsStructural(char c) {
	CharType charType = charTable.table[c];
	return charType == CharType::Space || charType == CharType::Quote || charType == CharType::Operator;
}

static constexpr bool IsDigit(char c) {
	return charTable.table[c] == CharType::Digit;
}
//--------------------------------------------------------------------------------------------------

struct Ctx {
	Mem         mem;
	char const* data;
	char const* end;
	Str const*  elemIter;
	Str const*  elemEnd;
};

//--------------------------------------------------------------------------------------------------

static Res<Ctx> CreateCtx(Mem mem, Str json) {
	Ctx ctx = {
		.mem      = mem,
		.data     = json.data,
		.end      = json.data + json.len,
		.elemIter = nullptr,
		.elemEnd  = nullptr,
	};

	Array<Str> elems(mem, 256);

	// TODO: Replace with SIMD scanning
	// Use vpshufb-based vectorized classification
	// Can classify 32 bytes per instruction into: structural, whitespace, quote, backslash, other
	char const* iter = ctx.data;
	while (iter < ctx.end) {
		CharType charType = charTable.table[*iter];
		switch (charType) {
			case CharType::Space:
				iter++;
				continue;

			case CharType::Operator:
				elems.Add(Str(iter, 1));
				iter++;
				break;

			case CharType::Quote: {
				elems.Add(Str(iter, 1));
				iter++;
				char const* begin = iter;
				for (;;) {
					if (iter >= ctx.end) { return Err_MissingClosingQuote("pos", iter - ctx.data); }
					char const c = *iter;
					if (c == '"') {
						elems.Add(Str(begin, (U32)(iter - begin)));
						elems.Add(Str(iter, 1));
						iter++;
						break;
					}
					iter++;
					if (c == '\\') {
						if (iter > ctx.end) { return Err_BadEscChar("pos", iter - ctx.data); }
						iter++;
					}
				}
				break;
			}

			case CharType::Other:      [[fallthrough]];
			case CharType::Underscore: [[fallthrough]];
			case CharType::Alpha:      [[fallthrough]];
			case CharType::Digit: {
				char const* begin = iter;
				do {
					iter++;
				} while (iter < ctx.end && IsName(*iter));
				elems.Add(Str(begin, (U32)(iter - begin)));
				break;
			}
		}
	}

	ctx.elemIter = elems.data;
	ctx.elemEnd  = elems.data + elems.len;

	return ctx;
}

//--------------------------------------------------------------------------------------------------

static U32 Pos(Ctx* ctx, Str elem) {
	return (U32)(elem.data - ctx->data);
}

//--------------------------------------------------------------------------------------------------

static Res<> Expect(Ctx* ctx, char expected) {
	if (ctx->elemIter >= ctx->elemEnd) {
		return Err_Eof("expected", expected);
	}
	Str actual = *ctx->elemIter++;
	if (actual[0] != expected) {
		return Err_Unexpected("pos", Pos(ctx, actual), "expected", expected, "actual", actual);
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static bool Maybe(Ctx* ctx, char maybe) {
	if (ctx->elemIter < ctx->elemEnd && ctx->elemIter->data[0] == maybe) {
		ctx->elemIter++;
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------

static Res<Str> Read(Ctx* ctx) {
	if (ctx->elemIter >= ctx->elemEnd) { return Err_Eof(); }
	return *ctx->elemIter++;
}

//--------------------------------------------------------------------------------------------------

static Res<bool> ParseBool(Ctx* ctx) {
	Str str; TryTo(Read(ctx), str);
	if (str == "true") { return true; }
	else if (str == "false") { return false; }
	else { return Err_BadBool("pos", Pos(ctx, str)); }
}

//--------------------------------------------------------------------------------------------------

static Res<I64> ParseI64(Ctx* ctx) {
	Str str; TryTo(Read(ctx), str);
	char const* iter = str.data;
	char const* end  = str.data + str.len;
	I64 sign = 1;
	if (*iter == '-') {
		sign = -1;
		iter++;
		if (iter >= end) { return Err_BadInt("pos", Pos(ctx, str)); }
	}
	I64 val = 0;
	do {
		if (!IsDigit(*iter)) { return Err_BadInt("pos", Pos(ctx, str)); }
		val = (val * 10) + (I64)(*iter - '0');
		iter++;
	} while (iter < end);
	return sign * val;
}

static Res<I32> ParseI32(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (I32)val; }
static Res<U64> ParseU64(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (U64)val; }
static Res<U32> ParseU32(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (U32)val; }

//--------------------------------------------------------------------------------------------------

static Res<F64> ParseF64(Ctx* ctx) {
	Str str; TryTo(Read(ctx), str);
	char const* iter = str.data;
	char const* end  = str.data + str.len;
	F64 sign = 1.0;
	if (*iter == '-') {
		sign = -1.0;
		iter++;
	}

	if (iter >= end || !IsDigit(*iter)) { return Err_BadFloat("pos", Pos(ctx, str)); }
	I64 intVal = 0;
	do {
		intVal = (intVal * 10) + (I64)(*iter - '0');
		iter++;
	} while (iter < end && IsDigit(*iter));

	I64 fracVal = 0;
	F64 fracDenom = 1.0;
	if (iter < end && *iter == '.') {
		iter++;
		if (iter >= end || !IsDigit(*iter)) { return Err_BadFloat("pos", Pos(ctx, str)); }
		do {
			fracVal = (fracVal * 10) + (I64)(*iter - '0');
			fracDenom *= 10.0;
			iter++;
		}
		while (iter < end && IsDigit(*iter));
	}

	F64 expSign = 1.0;
	I32 exp = 0;
	if (iter < end && (*iter == 'e' || *iter == 'E')) {
		iter++;
		if (iter < end) {
			if (*iter == '+') {
				iter++;
			} else if (*iter == '-') {
				expSign = -1.0;
				iter++;
			}
		}
		if (iter >= end || !IsDigit(*iter)) {  return Err_BadSci("pos", iter - ctx->data); }
		do {
			exp = (exp * 10) + (I64)(*iter - '0');
			iter++;
		} while (iter < end && IsDigit(*iter));
	}

	if (iter != end) {  return Err_BadFloat("pos", iter - ctx->data); }

	return sign * ((F64)intVal + ((F64)fracVal / fracDenom)) * pow(10.0, expSign * (F64)exp);
}

static Res<F32> ParseF32(Ctx* ctx) { F64 val; TryTo(ParseF64(ctx), val); return (F32)val; }

//--------------------------------------------------------------------------------------------------

static Res<Str> UnescapeAndIntern(Ctx* ctx, Str str) {
	if (str.len == 0) { return Str(); }

	MemScope memScope(ctx->mem);
	char*       unescaped     = Mem::AllocT<char>(ctx->mem, str.len);
	char*       unescapedIter = unescaped;
	char const* iter          = str.data;
	char const* end           = str.data + str.len;
	while (iter < end) {
		if (*iter != '\\') {
			*unescapedIter++ = *iter;
		} else {
			iter++;
			switch (*iter) {
				case '"':  *unescapedIter++ = '"';  break;
				case '\\': *unescapedIter++ = '\\'; break;
				case '/':  *unescapedIter++ = '/';  break;
				case 'b':  *unescapedIter++ = '\b'; break;
				case 'f':  *unescapedIter++ = '\f'; break;
				case 'n':  *unescapedIter++ = '\n'; break;
				case 'r':  *unescapedIter++ = '\r'; break;
				case 't':  *unescapedIter++ = '\t'; break;
				default: return Err_BadEscChar("pos", *iter);
			}
		}
		iter++;
	}
	return StrDb::Intern(Str(unescaped, (U32)(unescapedIter - unescaped)));
}

//--------------------------------------------------------------------------------------------------

static Res<Str> ParseStr(Ctx* ctx) {
	Try(Expect(ctx, '"'));
	Str str; TryTo(Read(ctx), str);
	TryTo(UnescapeAndIntern(ctx, str), str);
	Try(Expect(ctx, '"'));
	return str;
}

//--------------------------------------------------------------------------------------------------

static Res<Str> ParseName(Ctx* ctx) {
	Str str; TryTo(Read(ctx), str);
	if (str[0] == '"') {
		TryTo(Read(ctx), str);
		TryTo(UnescapeAndIntern(ctx, str), str);
		Try(Expect(ctx, '"'));
		return str;

	} else {
		char const* iter = str.data;
		char const* end  = str.data + str.len;
		while (iter < end) {
			if (!IsName(*iter)) {
				return Err_BadName("pos", Pos(ctx, str));
			}
			iter++;
		}
		return StrDb::Intern(str);
	}
}

//--------------------------------------------------------------------------------------------------

static U64 CalcArrayLen(Ctx* ctx) {
	Str const* elemIter = ctx->elemIter;
	Assert(elemIter->data[0] != '[');
	U64 len = 0;
	U32 depth = 0;
	for (;;) {
		char const c = elemIter->data[0];
		elemIter++;
		if (depth == 0) {
			if (c == '[' || c == '{') {
				depth++;
				len++;
			} else if (c == ']' || c == '}') {
				break;
			} else if (c == ',') {
				// skip
			} else if (c == '"') {
				len++;
				elemIter++;	// skip the paired closing-quote token
			} else {
				len++;
			}
		} else {
			if (c == '[' || c == '{') {
				depth++;
			} else if (c == ']' || c == '}') {
				depth--;
			}
		}
	}
	return len;
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseVal(Ctx* ctx, Traits const* traits, U8* out);

static Res<> ParseArray(Ctx* ctx, Traits const* traits, U8* out) {
	Try(Expect(ctx, '['));

	U64 len = CalcArrayLen(ctx);

	Traits elemTraits = *traits;
	elemTraits.arrayDepth--;
	U8* data = 0;
	if (len > 0) {
		U32 elemSize = elemTraits.arrayDepth == 0 ? elemTraits.size : sizeof(Span<U8>);
		data = Mem::AllocT<U8>(ctx->mem, len * elemSize);
		U8* elemOut = data;
		for (U32 i = 0; i < len - 1; i++) {
			Try(ParseVal(ctx, &elemTraits, elemOut));
			Try(Expect(ctx, ','));
			elemOut += elemSize;
		}
		Try(ParseVal(ctx, &elemTraits, elemOut));
		Maybe(ctx, ',');
	}
	Try(Expect(ctx, ']'));
	*(Span<U8>*)out = Span<U8>(data, len);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseObject(Ctx* ctx, Traits const* traits, U8* out);

static Res<> ParseVal(Ctx* ctx, Traits const* traits, U8* out) {
	if (traits->arrayDepth > 0) {
		return ParseArray(ctx, traits, out);
	}
	switch (traits->type) {
		case Type::Bool: return ParseBool(ctx).To(*(bool*)out);
		case Type::U32:  return ParseU32 (ctx).To(*(U32 *)out);
		case Type::U64:  return ParseU64 (ctx).To(*(U64 *)out);
		case Type::I32:  return ParseI32 (ctx).To(*(I32 *)out);
		case Type::I64:  return ParseI64 (ctx).To(*(I64 *)out);
		case Type::F32:  return ParseF32 (ctx).To(*(F32 *)out);
		case Type::F64:  return ParseF64 (ctx).To(*(F64 *)out);
		case Type::Str:  return ParseStr (ctx).To(*(Str *)out);
		case Type::Obj:  return ParseObject(ctx, traits, out);
		default: Panic("Unhandled Json::Type: %u", (U32)traits->type);
	}
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseObject(Ctx* ctx, Traits const* traits, U8* out) {
	Try(Expect(ctx, '{'));
	Span<Member const> members = traits->members;
	for (U64 i = 0; i < members.len; i++) {
		Member const* member = &members[i];
		Str name; TryTo(ParseName(ctx), name);
		while (member->name != name) {
			if (!member->optional) { return Err_MissingMember("expected", member->name, "actual", name); }
			i++;
			if (i >= members.len) { return Err_UnknownMember("name", name); }
			member = &members[i];
		}
		Try(Expect(ctx, ':'));
		Try(ParseVal(ctx, member->traits, out + member->offset));

		if (i < members.len - 1) {
			Try(Expect(ctx, ','));
		}
	}
	Maybe(ctx, ',');
	Try(Expect(ctx, '}'));
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> JsonToObjectImpl(Mem mem, Str json, Traits const* traits, U8* out) {
	Ctx ctx; TryTo(CreateCtx(mem, json), ctx);
	return ParseObject(&ctx, traits, out);
}

//--------------------------------------------------------------------------------------------------

Unit_Test("Json") {
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json