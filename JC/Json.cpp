#include "JC/Json.h"
#include "JC/StrDb.h"
#include "JC/Unit.h"
#include <math.h>

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, MissingClosingQuote);
DefErr(Json, Unexpected);
DefErr(Json, BadEscChar);
DefErr(Json, BadBool);
DefErr(Json, BadInt);
DefErr(Json, BadFloat);
DefErr(Json, BadSci);
DefErr(Json, Eof);

//--------------------------------------------------------------------------------------------------

struct Ctx {
	Mem         permMem;
	Mem         tempMem;
	const char* json;
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

//--------------------------------------------------------------------------------------------------

static void AddStructPos(Ctx* ctx, U32 pos) {
	if (ctx->structPosEnd == ctx->structPosEndCap) {
		const U32 cap = (U32)(ctx->structPosEndCap - ctx->structPos);
		const U32 newCap = cap ? cap * 2 : 256;
		const U32 len = (U32)(ctx->structPosEnd - ctx->structPos);
		ctx->structPos = Mem::ExtendT<U32>(ctx->tempMem, ctx->structPos, newCap);
		ctx->structPosEnd = ctx->structPos + len;	// bleh
		ctx->structPosEndCap = ctx->structPosEnd + newCap;
		ctx->structPosIter = ctx->structPos;	// bleh
	}
	*ctx->structPosEnd++ = pos;
}

//--------------------------------------------------------------------------------------------------

static Res<> Expect(Ctx* ctx, char expected) {
	const U32 pos = *ctx->structPosIter;
	const char actual = ctx->json[pos];
	if (actual != expected) {
		return Err_Unexpected("pos", pos, "expected", expected, "actual", actual ? Str(&actual, 1) : "eof");
	}
	ctx->structPosIter++;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

// Returns a pointer to the terminating quote
static Res<const char*> ScanStr(const char* json, const char* iter) {
	Assert(*iter == '"');
	for (iter++;;) {
		const char c = *iter;
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
	// SIMD OPPORTUNITY
	// Replace with SIMD scanning
	// Use vpshufb-based vectorized classification
	// Can classify 32 bytes per instruction into: structural, whitespace, quote, backslash, other
	const char* iter = ctx->json;
	for (;;) {
		const char c = *iter;
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
	const char*       iter = ctx->json + *ctx->structPosIter; ctx->structPosIter++;
	const char* const end  = ctx->json + *ctx->structPosIter;
	const U32 len = (U32)(end - iter);
	if (*iter == 't') {
		if (
			len < 4 ||
			*iter++ != 'r' ||
			*iter++ != 'u' ||
			*iter++ != 'e'
		) {
			return Err_BadBool("pos", iter - ctx->json);
		}
		if (IsNormal(*iter)) { return Err_BadBool("pos", iter - ctx->json); }
	}
	if (*iter == 'f') {
		if (
			len < 5 ||
			*iter++ != 'a' ||
			*iter++ != 'l' ||
			*iter++ != 's' ||
			*iter++ != 'e'
		) {
			return Err_BadBool("pos", iter - ctx->json);
		}
		if (IsNormal(*iter)) { return Err_BadBool("pos", iter - ctx->json); }
	}
	return Err_BadBool("pos", iter - ctx->json);
}

//--------------------------------------------------------------------------------------------------

static Res<I64> ParseI64(Ctx* ctx) {
	if (ctx->structPosIter >= ctx->structPosEnd) { return Err_Eof("expected", "int"); }
	const char* iter = ctx->json + *ctx->structPosIter; ctx->structPosIter++;
	I64 sign = 1;
	if (*iter == '-') {
		sign = -1;
		iter++;
	}
	if (!IsDigit(*iter)) { Err_BadFloat("pos", iter - ctx->json); }
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
	const char* iter = ctx->json + *ctx->structPosIter; ctx->structPosIter++;
	F64 sign = 1.0;
	if (*iter == '-') {
		sign = -1.0;
		iter++;
	}
	if (!IsDigit(*iter)) { Err_BadFloat("pos", iter - ctx->json); }
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
	const char* iter = ctx->json + *ctx->structPosIter + 1;
	Try(Expect(ctx, '"'));
	const char* const end = ctx->json + *ctx->structPosIter - 1;

	MemScope memScope(ctx->tempMem);
	char* data = Mem::AllocT<char>(ctx->tempMem, end - iter);
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
				case 'b':  *dataIter++ = 'b';  break;
				case 'f':  *dataIter++ = 'f';  break;
				case 'n':  *dataIter++ = 'n';  break;
				case 'r':  *dataIter++ = 'r';  break;
				case 't':  *dataIter++ = 't';  break;
				default: return Err_BadEscChar("pos", *iter);
			}
		}
		iter++;
	}

	return StrDb::Get(data, dataIter);
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseVal(Ctx* ctx, const Traits* traits, U8* out);

static Res<> ParseArray(Ctx* ctx, const Traits* traits, U8* out) {
	Try(Expect(ctx, '['));

	U32* const saved = ctx->structPosIter;
	U32 commaCount = 0;
	U32 depth = 0;
	for (;;) {
		const char c = ctx->json[*ctx->structPosIter];
		ctx->structPosIter++;
		if (c == '[' || c == '{') {
			depth++;
		} else if (c == ']' || c == '}') {
			if (depth == 0) {
				break;
			}
			depth--;
		} else if (c == ',' && depth == 0) {
			commaCount++;
		}
	}
	ctx->structPosIter = saved;
	const U32 elemCount = commaCount > 0 ? commaCount + 1 : 0;
	Traits elemTraits = *traits;
	elemTraits.arrayDepth--;
	U8* elemData = 0;
	if (elemCount > 0) {
		const U32 elemSize = elemTraits.arrayDepth == 0 ? elemTraits.size : sizeof(Span<U8>);
		elemData = Mem::AllocT<U8>(ctx->permMem, elemCount * elemSize);
		for (U32 i = 0; i < elemCount; i++) {
			U8* elemOut = elemData + (i * elemSize);
			Try(ParseVal(ctx, &elemTraits, elemOut));
			if (i < elemCount - 1) {
				Try(Expect(ctx, ','));
			}
		}
	}
	Try(Expect(ctx, ']'));

	*(Span<U8>*)out = Span<U8>(elemData, elemCount);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseObj(Ctx* ctx, Span<const Member> members, U8* out) {
	Try(Expect(ctx, '{'));
	for (U64 i = 0; i < members.len; i++) {
		const Member* member = &members[i];
		Str name; TryTo(ParseStr(ctx), name);
		if (name != member->name) {
			return Err_Unexpected("expected", member->name, "actual", name);
		}
		Try(Expect(ctx, ':'));
		Try(ParseVal(ctx, &member->traits, out + member->offset));

		if (i < members.len - 1) {
			Try(Expect(ctx, ','));
		}
	}
	return Expect(ctx, '}');
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseVal(Ctx* ctx, const Traits* traits, U8* out) {
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
		case Type::Obj:  return ParseObj (ctx, traits->members, out);
		default: Panic("Bad Json::Type: %u", (U32)traits->type);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> ObjFromJson(Mem permMem, Mem tempMem, const char* json, U32 jsonLen, Span<const Member> members, U8* out) {
	U64 mark = Mem::Mark(tempMem);
	Defer { if (permMem != tempMem) { Mem::Reset(tempMem, mark); } };

	Ctx ctx = {
		.permMem  = permMem,
		.tempMem  = tempMem,
		.json     = json,
		.jsonLen  = jsonLen,
	};
	Try(Scan(&ctx));

	const Traits traits = { .type = Type::Obj, .size = 0, .arrayDepth = 0, .members = members };
	return ParseObj(&ctx, members, out);
}

//--------------------------------------------------------------------------------------------------

struct Foo {
	Str str;
	F64 f;
	Span<Span<I32>> ints;
};
Json_Begin(Foo)
	Json_Member("str", str)
	Json_Member("f", f)
	Json_Member("ints", ints)
Json_End(Foo)

Unit_Test("Json") {
	Unit_SubTest("simple") {
		constexpr const char* json = "{ \"str\": \"hello world\", \"f\": 1.25, \"ints\": [ [ 1, 3, 5 ], [ 7, 8 ], [], [ 0, 1, 2, 3, 4 ] ] }";
		Foo foo;
		Unit_Check(FromJson(testMem, testMem, json, StrLen(json), &foo));
		Unit_CheckEq(foo.str, "hello world");
		Unit_CheckEq(foo.f, 1.25);
		Unit_CheckEq(foo.ints.len, 4);
		Unit_CheckSpanEq(foo.ints[0], Span<I32>({ 1, 3, 5 }));
		Unit_CheckSpanEq(foo.ints[1], Span<I32>({ 7, 8  }));
		Unit_CheckSpanEq(foo.ints[2], Span<I32>({ }));
		Unit_CheckSpanEq(foo.ints[3], Span<I32>({ 0, 1, 2, 3, 4 }));
	}
/*
	SubTest("No Trailing Commas Object") {
		const Str json = "{a:1,b:2}";
		CheckTrue(Parse(testAllocator, testAllocator, json).To(doc));
		Obj obj = GetObj(doc, GetRoot(doc));
		CheckEq(obj.keys.len, 2);
		CheckEq(obj.vals.len, 2);
		CheckEq(GetStr(doc, obj.keys[0]), "a");
		CheckEq(GetU64(doc, obj.vals[0]), 1);
		CheckEq(GetStr(doc, obj.keys[1]), "b");
		CheckEq(GetU64(doc, obj.vals[1]), 2);
	}

	SubTest("Trailing Commas Object") {
		const Str json = "{a:1,b:2,}";
		CheckTrue(Parse(testAllocator, testAllocator, json).To(doc));
		Obj obj = GetObj(doc, GetRoot(doc));
		CheckEq(obj.keys.len, 2);
		CheckEq(obj.vals.len, 2);
		CheckEq(GetStr(doc, obj.keys[0]), "a");
		CheckEq(GetU64(doc, obj.vals[0]), 1);
		CheckEq(GetStr(doc, obj.keys[1]), "b");
		CheckEq(GetU64(doc, obj.vals[1]), 2);
	}

	SubTest("No Trailing Commas Array") {
		const Str json = "[1,2,3]";
		CheckTrue(Parse(testAllocator, testAllocator, json).To(doc));
		Span<Elem> arr = GetArr(doc, GetRoot(doc));
		CheckEq(arr.len, 3);
		CheckEq(GetU64(doc, arr[0]), 1);
		CheckEq(GetU64(doc, arr[1]), 2);
		CheckEq(GetU64(doc, arr[2]), 3);
	}

	SubTest("Trailing Commas Array") {
		const Str json = "[1, 2, 3,]";
		CheckTrue(Parse(testAllocator, testAllocator, json).To(doc));
		Span<Elem> arr = GetArr(doc, GetRoot(doc));
		CheckEq(arr.len, 3);
		CheckEq(GetU64(doc, arr[0]), 1);
		CheckEq(GetU64(doc, arr[1]), 2);
		CheckEq(GetU64(doc, arr[2]), 3);
	}

	SubTest("Big Complex Object") {
		const Str json = "{"
			"foo: \"hello\","
			"\"foo bar\": 1.25,"
			"\"arr\": ["
				"\"qux\","
				"456,"
				"{"
					"\"another\": \"key\","
					"\"day\": 1,"
				"}"
			"]"
		"}";
		CheckTrue(Parse(testAllocator, testAllocator, json).To(doc));
		Obj obj = GetObj(doc, GetRoot(doc));
		CheckEq(obj.keys.len, 3);
		CheckEq(obj.vals.len, 3);
		CheckEq(GetStr(doc, obj.keys[0]), "foo");
		CheckEq(GetStr(doc, obj.vals[0]), "hello");
		CheckEq(GetStr(doc, obj.keys[1]), "foo bar");
		CheckEq(GetF64(doc, obj.vals[1]), 1.25);
		CheckEq(GetStr(doc, obj.keys[2]), "arr");
		Span<Elem> arr = GetArr(doc, obj.vals[2]);
		CheckEq(arr.len, 3);
		CheckEq(GetStr(doc, arr[0]), "qux");
		CheckEq(GetU64(doc, arr[1]), 456);
		Obj subObj = GetObj(doc, arr[2]);
		CheckEq(subObj.keys.len, 2);
		CheckEq(subObj.vals.len, 2);
		CheckEq(GetStr(doc, subObj.keys[0]), "another");
		CheckEq(GetStr(doc, subObj.vals[0]), "key");
		CheckEq(GetStr(doc, subObj.keys[1]), "day");
		CheckEq(GetU64(doc, subObj.vals[1]), 1);
	}

	Free(doc);

	// TODO: more detailed tests, especially error conditions
	*/
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json