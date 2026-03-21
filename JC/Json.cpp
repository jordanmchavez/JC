#include "JC/Json.h"

#include "JC/Array.h"
#include "JC/StrDb.h"
#include "JC/UnitTest.h"
#include <math.h>

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, MissingClosingQuote);
DefErr(Json, Unexpected);
DefErr(Json, BadEscChar);
DefErr(Json, BadName);
DefErr(Json, BadBool);
DefErr(Json, BadInt);
DefErr(Json, BadFloat);
DefErr(Json, BadSci);
DefErr(Json, Eof);
DefErr(Json, ArrayMaxLen);

//--------------------------------------------------------------------------------------------------

struct Ctx {
	Mem         mem;
	char const* json;
	U32         jsonLen;
	U32*        structPos = 0;
	U32*        structPosEnd = 0;
	U32*        structPosEndCap = 0;
	U32*        structPosIter = 0;
};

//--------------------------------------------------------------------------------------------------

enum struct CharType : U8 {
	Normal = 0,
	Space  = 1,
	Quote  = 2,
	Op     = 3,
	Eof    = 4,
};

struct CharTable {
	CharType data[256] = {};
	constexpr CharTable() {
		data[' ' ] = CharType::Space;
		data['\t'] = CharType::Space;
		data['\r'] = CharType::Space;
		data['\n'] = CharType::Space;
		data['"' ] = CharType::Quote;
		data['{' ] = CharType::Op;
		data['}' ] = CharType::Op;
		data['[' ] = CharType::Op;
		data[']' ] = CharType::Op;
		data[':' ] = CharType::Op;
		data[',' ] = CharType::Op;
		data['\0'] = CharType::Eof;
	}
};
constexpr CharTable charTable;

static constexpr bool IsDigit(char c) { return c >= '0' && c <= '9'; }
static constexpr bool IsNormal(char c) { return charTable.data[c] == CharType::Normal; }
// TODO: IsNormal() and IsDigit() tables

//--------------------------------------------------------------------------------------------------

static void AddStructPos(Ctx* ctx, U32 pos) {
	U32 const cap = (U32)(ctx->structPosEndCap - ctx->structPos);
	U32 const len = (U32)(ctx->structPosEnd - ctx->structPos);
	if (ctx->structPosEnd == ctx->structPosEndCap) {
		U32 const newCap     = cap ? cap * 2 : 256;
		// TODO: replace this whole thing with Array
		ctx->structPos       = Mem::ReallocT<U32>(ctx->mem, ctx->structPos, cap, newCap);
		ctx->structPosEnd    = ctx->structPos + len;	// bleh
		ctx->structPosEndCap = ctx->structPos + newCap;
		ctx->structPosIter   = ctx->structPos;	// bleh
	}
	*ctx->structPosEnd++ = pos;
}

//--------------------------------------------------------------------------------------------------

static Res<> Expect(Ctx* ctx, char expected) {
	U32 const pos = *ctx->structPosIter;
	char const actual = ctx->json[pos];
	if (actual != expected) {
		return Err_Unexpected("pos", pos, "expected", expected, "actual", actual ? Str(&actual, 1) : "eof");
	}
	ctx->structPosIter++;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static bool Maybe(Ctx* ctx) {
	if (ctx->json[*ctx->structPosIter] == ',') {
		ctx->structPosIter++;
	}
}

//--------------------------------------------------------------------------------------------------

// Returns a pointer to the terminating quote
static Res<char const*> ScanStr(char const* json, char const* iter) {
	Assert(*iter == '"');
	iter++;
	for (;;) {
		char const c = *iter;
		if (c == '\0') { return Err_MissingClosingQuote("pos", iter - json); }
		if (c == '"') { return iter; }
		iter++;
		if (c == '\\') {
			if (*iter == '\0') { return Err_BadEscChar("pos", iter - json); }
			iter++;
		}
	}
}

//--------------------------------------------------------------------------------------------------

static Res<> Scan(Ctx* ctx) {
	// TODO: Replace with SIMD scanning
	// Use vpshufb-based vectorized classification
	// Can classify 32 bytes per instruction into: structural, whitespace, quote, backslash, other
	char const* iter = ctx->json;
	for (;;) {
		char const c = *iter;
		switch (charTable.data[c]) {
			case CharType::Normal:
				AddStructPos(ctx, (U32)(iter - ctx->json));
				do {
					iter++;
				} while (charTable.data[*iter] == CharType::Normal);
				break;
			case CharType::Space:
				iter++;
				continue;
			case CharType::Quote:
				AddStructPos(ctx, (U32)(iter - ctx->json));
				TryTo(ScanStr(ctx->json, iter), iter);
				Assert(*iter == '"');
				AddStructPos(ctx, (U32)(iter - ctx->json));
				iter++;
				break;
			case CharType::Op:
				AddStructPos(ctx, (U32)(iter - ctx->json));
				iter++;
				break;
			case CharType::Eof:
				AddStructPos(ctx, (U32)(iter - ctx->json));
				iter++;
				return Ok();
			default:
				Panic("Unhandled CharType: %u", (U32)charTable.data[c]);
		}
	}
}

//--------------------------------------------------------------------------------------------------

static Res<bool> ParseBool(Ctx* ctx) {
	if (ctx->structPosIter >= ctx->structPosEnd) { return Err_Eof("expected", "bool"); }
	char const*       iter = ctx->json + *ctx->structPosIter; ctx->structPosIter++;
	char const* const end  = ctx->json + *ctx->structPosIter;
	U32 const len = (U32)(end - iter);
	if (*iter == 't') {
		iter++;
		if (len < 4) { return Err_BadBool("pos", iter - ctx->json); }
		if (*iter++ != 'r') { return Err_BadBool("pos", iter - ctx->json); }
		if (*iter++ != 'u') { return Err_BadBool("pos", iter - ctx->json); }
		if (*iter++ != 'e') { return Err_BadBool("pos", iter - ctx->json); }
		if (IsNormal(*iter)) { return Err_BadBool("pos", iter - ctx->json); }
		return true;
	}
	if (*iter == 'f') {
		iter++;
		if (len < 5) { return Err_BadBool("pos", iter - ctx->json); }
		if (*iter++ != 'a') { return Err_BadBool("pos", iter - ctx->json); }
		if (*iter++ != 'l') { return Err_BadBool("pos", iter - ctx->json); }
		if (*iter++ != 's') { return Err_BadBool("pos", iter - ctx->json); }
		if (*iter++ != 'e') { return Err_BadBool("pos", iter - ctx->json); }
		if (IsNormal(*iter)) { return Err_BadBool("pos", iter - ctx->json); }
		return false;
	}
	return Err_BadBool("pos", iter - ctx->json);
}

//--------------------------------------------------------------------------------------------------

static Res<I64> ParseI64(Ctx* ctx) {
	if (ctx->structPosIter >= ctx->structPosEnd) { return Err_Eof("expected", "int"); }
	char const* iter = ctx->json + *ctx->structPosIter; ctx->structPosIter++;
	I64 sign = 1;
	if (*iter == '-') {
		sign = -1;
		iter++;
	}
	if (!IsDigit(*iter)) { return Err_BadInt("pos", iter - ctx->json); }
	I64 val = (I64)(*iter - '0');
	iter++;
	while (IsDigit(*iter)) {
		val = (val * 10) + (I64)(*iter - '0');
		iter++;
	};
	if (IsNormal(*iter)) { return Err_BadInt("pos", iter - ctx->json); }
	return sign * val;
}

static Res<I32> ParseI32(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (I32)val; }
static Res<U64> ParseU64(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (U64)val; }
static Res<U32> ParseU32(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (U32)val; }

//--------------------------------------------------------------------------------------------------

static Res<F64> ParseF64(Ctx* ctx) {
	if (ctx->structPosIter >= ctx->structPosEnd) { return Err_Eof("expected", "float"); }
	char const* iter = ctx->json + *ctx->structPosIter; ctx->structPosIter++;
	F64 sign = 1.0;
	if (*iter == '-') {
		sign = -1.0;
		iter++;
	}
	if (!IsDigit(*iter)) { return Err_BadFloat("pos", iter - ctx->json); }
	U64 intVal = (U64)(*iter - '0');
	iter++;
	while (IsDigit(*iter)) {
		intVal = (intVal * 10) + (U64)(*iter - '0');
		iter++;
	}

	U64 fracVal = 0;
	F64 fracDenom = 1.0;
	if (*iter == '.') {
		iter++;
		while (IsDigit(*iter)) {
			fracVal = (fracVal * 10) + (*iter - '0');
			fracDenom *= 10.0;
			iter++;
		}
	}

	F64 expSign = 1.0;
	U32 exp = 0;
	if (*iter == 'e' || *iter == 'E') {
		iter++;
		if (*iter == '+') {
			iter++;
		} else if (*iter == '-') {
			expSign = -1.0;
			iter++;
		}
		if (!IsDigit(*iter)) {  return Err_BadSci("pos", iter - ctx->json); }
		exp = *iter - '0';
		iter++;
		while (IsDigit(*iter)) {
			exp = (exp * 10) + (*iter - '0');
			iter++;
		}
	}

	if (IsNormal(*iter)) { return Err_BadFloat("pos", iter - ctx->json); }

	return sign * ((F64)intVal + ((F64)fracVal / fracDenom)) * pow(10.0, expSign * (F64)exp);
}

static Res<F32> ParseF32(Ctx* ctx) { F64 val; TryTo(ParseF64(ctx), val); return (F32)val; }

//--------------------------------------------------------------------------------------------------

static Res<Str> ParseStr(Ctx* ctx) {
	char const* iter = ctx->json + *ctx->structPosIter + 1;
	Try(Expect(ctx, '"'));
	char const* const end = ctx->json + *ctx->structPosIter;
	Try(Expect(ctx, '"'));

	if (iter == end) { return Str(); }

	MemScope memScope(ctx->mem);
	char* data = Mem::AllocT<char>(ctx->mem, end - iter);
	char* dataIter = data;
	while (iter < end) {
		if (*iter != '\\') {
			*dataIter++ = *iter;
		} else {
			iter++;
			switch (*iter) {
				case '"':  *dataIter++ = '"';  break;
				case '\\': *dataIter++ = '\\'; break;
				case '/':  *dataIter++ = '/';  break;
				case 'b':  *dataIter++ = '\b';  break;
				case 'f':  *dataIter++ = '\f';  break;
				case 'n':  *dataIter++ = '\n';  break;
				case 'r':  *dataIter++ = '\r';  break;
				case 't':  *dataIter++ = '\t';  break;
				default: return Err_BadEscChar("pos", *iter);
			}
		}
		iter++;
	}

	return StrDb::Intern(Str(data, (U32)(dataIter - data)));
}

//--------------------------------------------------------------------------------------------------

static constexpr bool IsName(char c) {
	return
		(c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
}

static Res<Str> ParseName(Ctx* ctx) {
	char const* iter = ctx->json + *ctx->structPosIter;
	if (*iter == '"') { return ParseStr(ctx); }

	ctx->structPosIter++;
	char const* const begin = iter;
	while (IsName(*iter)) {
		iter++;
	}
	if (begin == iter || IsNormal(*iter)) { return Err_BadName("pos", begin - ctx->json); }
	return StrDb::Intern(Str(begin, (U32)(iter - begin)));
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseVal(Ctx* ctx, Traits* traits, U8* out) {
	if (traits->arrayDepth > 0) {
		U64* arrayLen   = (U64*)out;
		U8*  arrayData  = out + 8;
		return ParseArray(ctx, traits, arrayData, arrayLen);
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

static U64 CalcArrayLen(Ctx* ctx) {
	U32* const saved = ctx->structPosIter;
	U64 len = 0;
	U32 depth = 0;
	for (;;) {
		char const c = ctx->json[*ctx->structPosIter];
		ctx->structPosIter++;
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
				ctx->structPosIter++;	// skip the paired closing-quote token
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
	ctx->structPosIter = saved;
	return len;
}

//--------------------------------------------------------------------------------------------------

Res<> ParseArray(Ctx* ctx, Traits* traits, U8* arrayData, U64* arrayLen) {
	Try(Expect(ctx, '['));

	U64 incLen = CalcArrayLen(ctx);
	if (*arrayLen + incLen > traits->arrayMaxLen) {
		return Err_ArrayMaxLen("pos", *ctx->structPosIter);
	}

	Traits elemTraits = *traits;
	U64  elemSize = elemTraits.arrayDepth == 0 ? elemTraits.size : sizeof(Array<U8, 1>);
	U8*  outIter  = arrayData + (*arrayLen * elemSize);
	elemTraits.arrayDepth--;
	for (U32 i = 0; i < incLen - 1; i++) {
		Try(ParseVal(ctx, &elemTraits, outIter));
		Try(Expect(ctx, ','));
		outIter += elemSize;
	}
	Try(ParseVal(ctx, &elemTraits, outIter));
	Maybe(ctx, ',');
	Try(Expect(ctx, ']'));

	*arrayLen += incLen;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseObject(Ctx* ctx, Traits* traits, U8* out) {
	Try(Expect(ctx, '{'));
	Span<Member> members = traits->members;
	for (U64 i = 0; i < members.len; i++) {
		Member* member = &members[i];
		Str memberName; TryTo(ParseName(ctx), memberName);
		if (memberName != member->name) {
			return Err_Unexpected("expected", member->name, "actual", memberName);
		}
		Try(Expect(ctx, ':'));
		Try(ParseVal(ctx, &member->traits, out + member->offset));

		if (i < members.len - 1) {
			Try(Expect(ctx, ','));
		}
	}
	Maybe(ctx, ',');
	Try(Expect(ctx, '}'));
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> JsonToObjectImpl(Mem mem, char const* json, U32 jsonLen, Traits* traits, U8* out) {
	Ctx ctx = {
		.mem     = mem,
		.json    = json,
		.jsonLen = jsonLen,
	};
	Try(Scan(&ctx));
	return ParseObject(&ctx, traits, out);
}

//--------------------------------------------------------------------------------------------------

Unit_Test("Json") {
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json