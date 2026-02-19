#include "JC/Json.h"
#include "JC/StrDb.h"
#include "JC/Unit.h"
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

//--------------------------------------------------------------------------------------------------

struct Ctx {
	Mem         permMem;
	Mem         tempMem;
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
		ctx->structPos       = Mem::ExtendT<U32>(ctx->tempMem, ctx->structPos, newCap);
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
	// SIMD OPPORTUNITY
	// Replace with SIMD scanning
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

	return StrDb::Get(Str(data, (U32)(dataIter - data)));
}

//--------------------------------------------------------------------------------------------------

static constexpr bool IsName(char c) {
	return
		(c >= '0' && c < '9') ||
		(c >= 'a' && c < 'z') ||
		(c >= 'A' && c < 'Z') ||
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
	return StrDb::Get(Str(begin, (U32)(iter - begin)));
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseVal(Ctx* ctx, const Traits* traits, U8* out);

static Res<> ParseArray(Ctx* ctx, const Traits* traits, U8* out) {
	Try(Expect(ctx, '['));

	U32* const saved = ctx->structPosIter;
	U32 elemCount = 0;
	U32 depth = 0;
	for (;;) {
		char const c = ctx->json[*ctx->structPosIter];
		ctx->structPosIter++;
		if (depth == 0) {
			if (c == '[' || c == '{') {
				depth++;
				elemCount++;
			} else if (c == ']' || c == '}') {
				break;
			} else if (c == ',') {
				// skip
			} else {
				elemCount++;
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
	Traits elemTraits = *traits;
	elemTraits.arrayDepth--;
	U8* elemData = 0;
	if (elemCount > 0) {
		U32 const elemSize = elemTraits.arrayDepth == 0 ? elemTraits.size : sizeof(Span<U8>);
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
		Str name; TryTo(ParseName(ctx), name);
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

Res<> ToObj(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, Span<const Member> members, U8* out) {
	MemMark mark = Mem::Mark(tempMem);
	Defer { if (permMem != tempMem) { Mem::Reset(tempMem, mark); } };
	Ctx ctx = {
		.permMem  = permMem,
		.tempMem  = tempMem,
		.json     = json,
		.jsonLen  = jsonLen,
	};
	Try(Scan(&ctx));
	return ParseObj(&ctx, members, out);
}

//--------------------------------------------------------------------------------------------------

Res<> ToArray(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, const Traits* traits, U8* out) {
	MemMark mark = Mem::Mark(tempMem);
	Defer { if (permMem != tempMem) { Mem::Reset(tempMem, mark); } };
	Ctx ctx = {
		.permMem  = permMem,
		.tempMem  = tempMem,
		.json     = json,
		.jsonLen  = jsonLen,
	};
	Try(Scan(&ctx));
	return ParseArray(&ctx, traits, out);
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

struct Bar {
	bool b;
	U32  u;
	Str  s;
};
Json_Begin(Bar)
	Json_Member("b", b)
	Json_Member("u", u)
	Json_Member("s", s)
Json_End(Bar)

Unit_Test("Json") {
	constexpr char const* fooJson = "{"
		"\"str\": \"hello world\","
		"\"f\": 1.25,"
		"\"ints\": [ [ 1, 3, 5 ], [ 7, 8 ], [], [ 0, 1, 2, 3, 4 ], [ 999 ] ]"
	"}";
	Foo foo;
	Unit_CheckRes(ToObj(testMem, testMem, fooJson, StrLen(fooJson), &foo));
	Unit_CheckEq(foo.str, "hello world");
	Unit_CheckEq(foo.f, 1.25);
	Unit_CheckEq(foo.ints.len, 5);
	Unit_CheckSpanEq(foo.ints[0], Span<I32>({ 1, 3, 5 }));
	Unit_CheckSpanEq(foo.ints[1], Span<I32>({ 7, 8  }));
	Unit_CheckSpanEq(foo.ints[2], Span<I32>({ }));
	Unit_CheckSpanEq(foo.ints[3], Span<I32>({ 0, 1, 2, 3, 4 }));
	Unit_CheckSpanEq(foo.ints[4], Span<I32>({ 999 }));

	constexpr char const* fooUnquotedMinifiedJson = "{"
		"str:\"hello world\","
		"f:1.25,"
		"ints:[[1,3,5],[7,8],[],[0,1,2,3,4],[999]]"
	"}";
	Unit_CheckRes(ToObj(testMem, testMem, fooUnquotedMinifiedJson, StrLen(fooUnquotedMinifiedJson), &foo));
	Unit_CheckEq(foo.str, "hello world");
	Unit_CheckEq(foo.f, 1.25);
	Unit_CheckEq(foo.ints.len, 5);
	Unit_CheckSpanEq(foo.ints[0], Span<I32>({ 1, 3, 5 }));
	Unit_CheckSpanEq(foo.ints[1], Span<I32>({ 7, 8  }));
	Unit_CheckSpanEq(foo.ints[2], Span<I32>({ }));
	Unit_CheckSpanEq(foo.ints[3], Span<I32>({ 0, 1, 2, 3, 4 }));
	Unit_CheckSpanEq(foo.ints[4], Span<I32>({ 999 }));

	constexpr char const* barsJson = "[ "
		"{ \"b\": true,  \"u\": 123, \"s\": \"\" },"
		"{ \"b\": false, \"u\": 456, \"s\": \"hello\" },"
		"{ \"b\": true,  \"u\": 001, \"s\": \"wor\\r\\n\\b\\\"\\t\\\\ ,:][{}ld\" }"
	"]";
	Span<Bar> bars;
	Unit_CheckRes(ToArray(testMem, testMem, barsJson, StrLen(barsJson), &bars));
	Unit_CheckEq(bars.len, 3);
	Unit_CheckEq(bars[0].b, true);
	Unit_CheckEq(bars[0].u, 123u);
	Unit_CheckEq(bars[0].s, "");
	Unit_CheckEq(bars[1].b, false);
	Unit_CheckEq(bars[1].u, 456u);
	Unit_CheckEq(bars[1].s, "hello");
	Unit_CheckEq(bars[2].b, true);
	Unit_CheckEq(bars[2].u, 1u);
	Unit_CheckEq(bars[2].s, "wor\r\n\b\"\t\\ ,:][{}ld");

	constexpr char const* nestedArraysJson = "[[[]]]";
	Span<Span<Span<U32>>> nestedArrays;
	Unit_CheckRes(ToArray(testMem, testMem, nestedArraysJson, StrLen(nestedArraysJson), &nestedArrays));
	Unit_CheckEq(nestedArrays.len, 1);
	Unit_CheckEq(nestedArrays[0].len, 1);
	Unit_CheckEq(nestedArrays[0][0].len, 0);
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