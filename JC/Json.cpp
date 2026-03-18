#include "JC/Json.h"
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
		// TODO: replace this whole thing with Array
		ctx->structPos       = Mem::ReallocT<U32>(ctx->tempMem, ctx->structPos, cap, newCap);
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
			} else if (c == '"') {
				elemCount++;
				ctx->structPosIter++;	// skip the paired closing-quote token
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
	// Optional trailing comma before ']'
	if (ctx->structPosIter < ctx->structPosEnd && ctx->json[*ctx->structPosIter] == ',') {
		ctx->structPosIter++;
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
	// Optional trailing comma before '}'
	if (ctx->structPosIter < ctx->structPosEnd && ctx->json[*ctx->structPosIter] == ',') {
		ctx->structPosIter++;
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

Res<> ToObjectImpl(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, Span<const Member> members, U8* out) {
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

Res<> ToArrayImpl(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, const Traits* traits, U8* out) {
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

struct Point {
	I32 x;
	I32 y;
};
Json_Begin(Point)
	Json_Member("x", x)
	Json_Member("y", y)
Json_End(Point)

struct Item {
	Str       name;
	I32       count;
	Span<F32> values;
};
Json_Begin(Item)
	Json_Member("name",   name)
	Json_Member("count",  count)
	Json_Member("values", values)
Json_End(Item)

struct Catalog {
	bool            active;
	U32             version;
	I32             offset;
	I64             bigNum;
	F64             ratio;
	Str             label;
	Span<I32>       ids;
	Span<Span<I32>> matrix;
	Span<Item>      items;
};
Json_Begin(Catalog)
	Json_Member("active",  active)
	Json_Member("version", version)
	Json_Member("offset",  offset)
	Json_Member("bigNum",  bigNum)
	Json_Member("ratio",   ratio)
	Json_Member("label",   label)
	Json_Member("ids",     ids)
	Json_Member("matrix",  matrix)
	Json_Member("items",   items)
Json_End(Catalog)

Unit_Test("Json") {

	Unit_SubTest("Bool") {
		Span<bool> vals;
		Unit_CheckRes(ToArray(testMem, testMem, "[ true, false, true ]", StrLen("[ true, false, true ]"), &vals));
		Unit_CheckEq(vals.len, 3u);
		Unit_CheckEq(vals[0], true);
		Unit_CheckEq(vals[1], false);
		Unit_CheckEq(vals[2], true);
	}

	Unit_SubTest("Integers") {
		Span<I32> vals;
		Unit_CheckRes(ToArray(testMem, testMem, "[ 0, 1, -1, 100, -999 ]", StrLen("[ 0, 1, -1, 100, -999 ]"), &vals));
		Unit_CheckEq(vals.len, 5u);
		Unit_CheckEq(vals[0], 0);
		Unit_CheckEq(vals[1], 1);
		Unit_CheckEq(vals[2], -1);
		Unit_CheckEq(vals[3], 100);
		Unit_CheckEq(vals[4], -999);
	}

	Unit_SubTest("Floats") {
		Span<F64> vals;
		constexpr char const* json = "[ 0.0, 1.5, -2.25, 1.5e2, 1.5E2, 2.5e-1 ]";
		Unit_CheckRes(ToArray(testMem, testMem, json, StrLen(json), &vals));
		Unit_CheckEq(vals.len, 6u);
		Unit_CheckEq(vals[0], 0.0);
		Unit_CheckEq(vals[1], 1.5);
		Unit_CheckEq(vals[2], -2.25);
		Unit_CheckEq(vals[3], 150.0);
		Unit_CheckEq(vals[4], 150.0);
		Unit_CheckEq(vals[5], 0.25);
	}

	Unit_SubTest("Strings") {
		Span<Str> vals;
		Unit_CheckRes(ToArray(testMem, testMem, "[ \"hello\", \"world\", \"\" ]", StrLen("[ \"hello\", \"world\", \"\" ]"), &vals));
		Unit_CheckEq(vals.len, 3u);
		Unit_CheckEq(vals[0], "hello");
		Unit_CheckEq(vals[1], "world");
		Unit_CheckEq(vals[2], "");
	}

	Unit_SubTest("String escapes") {
		Span<Str> vals;
		constexpr char const* json = "[ \"\\\"quote\\\"\", \"\\\\\", \"\\/\", \"\\b\\f\\n\\r\\t\" ]";
		Unit_CheckRes(ToArray(testMem, testMem, json, StrLen(json), &vals));
		Unit_CheckEq(vals.len, 4u);
		Unit_CheckEq(vals[0], "\"quote\"");
		Unit_CheckEq(vals[1], "\\");
		Unit_CheckEq(vals[2], "/");
		Unit_CheckEq(vals[3], "\b\f\n\r\t");
	}

	Unit_SubTest("Structural chars in strings") {
		// Verify the scanner does not misinterpret [ ] { } , : inside quoted strings.
		Span<Str> vals;
		constexpr char const* json = "[ \"a[b]c\", \"x:{y}\", \"p,q\", \"{[,:]}\" ]";
		Unit_CheckRes(ToArray(testMem, testMem, json, StrLen(json), &vals));
		Unit_CheckEq(vals.len, 4u);
		Unit_CheckEq(vals[0], "a[b]c");
		Unit_CheckEq(vals[1], "x:{y}");
		Unit_CheckEq(vals[2], "p,q");
		Unit_CheckEq(vals[3], "{[,:]}");
	}

	Unit_SubTest("Empty array") {
		Span<I32> vals;
		Unit_CheckRes(ToArray(testMem, testMem, "[ ]", StrLen("[ ]"), &vals));
		Unit_CheckEq(vals.len, 0u);
	}

	Unit_SubTest("Single-element array") {
		Span<I32> vals;
		Unit_CheckRes(ToArray(testMem, testMem, "[ 42 ]", StrLen("[ 42 ]"), &vals));
		Unit_CheckEq(vals.len, 1u);
		Unit_CheckEq(vals[0], 42);
	}

	Unit_SubTest("2D array") {
		// Inner arrays cover zero ([ ]), one ([ 3 ]), and many ([ 1, 2 ]) elements.
		Span<Span<I32>> vals;
		Unit_CheckRes(ToArray(testMem, testMem, "[ [ 1, 2 ], [ 3 ], [ ] ]", StrLen("[ [ 1, 2 ], [ 3 ], [ ] ]"), &vals));
		Unit_CheckEq(vals.len, 3u);
		Unit_CheckSpanEq(vals[0], Span<I32>({1, 2}));
		Unit_CheckSpanEq(vals[1], Span<I32>({3}));
		Unit_CheckSpanEq(vals[2], Span<I32>({}));
	}

	Unit_SubTest("3D array") {
		Span<Span<Span<I32>>> vals;
		Unit_CheckRes(ToArray(testMem, testMem, "[ [ [ 1, 2 ], [ 3 ] ], [ [ ] ] ]", StrLen("[ [ [ 1, 2 ], [ 3 ] ], [ [ ] ] ]"), &vals));
		Unit_CheckEq(vals.len, 2u);
		Unit_CheckEq(vals[0].len, 2u);
		Unit_CheckSpanEq(vals[0][0], Span<I32>({1, 2}));
		Unit_CheckSpanEq(vals[0][1], Span<I32>({3}));
		Unit_CheckEq(vals[1].len, 1u);
		Unit_CheckSpanEq(vals[1][0], Span<I32>({}));
	}

	Unit_SubTest("Object with quoted keys") {
		Point p;
		Unit_CheckRes(ToObject(testMem, testMem, "{ \"x\": 10, \"y\": -5 }", StrLen("{ \"x\": 10, \"y\": -5 }"), &p));
		Unit_CheckEq(p.x, 10);
		Unit_CheckEq(p.y, -5);
	}

	Unit_SubTest("Object with unquoted keys") {
		Point p;
		Unit_CheckRes(ToObject(testMem, testMem, "{ x: 10, y: -5 }", StrLen("{ x: 10, y: -5 }"), &p));
		Unit_CheckEq(p.x, 10);
		Unit_CheckEq(p.y, -5);
	}

	Unit_SubTest("Array of objects") {
		Span<Point> pts;
		constexpr char const* json = "[ { \"x\": 1, \"y\": 2 }, { \"x\": -3, \"y\": 0 } ]";
		Unit_CheckRes(ToArray(testMem, testMem, json, StrLen(json), &pts));
		Unit_CheckEq(pts.len, 2u);
		Unit_CheckEq(pts[0].x, 1);
		Unit_CheckEq(pts[0].y, 2);
		Unit_CheckEq(pts[1].x, -3);
		Unit_CheckEq(pts[1].y, 0);
	}

	Unit_SubTest("Whitespace") {
		// Spacious JSON with newlines and tabs.
		constexpr char const* spacious =
			"{\n"
			"\t\"x\" :\t10 ,\n"
			"\t\"y\" :\t-5\n"
			"}";
		Point ps;
		Unit_CheckRes(ToObject(testMem, testMem, spacious, StrLen(spacious), &ps));
		Unit_CheckEq(ps.x, 10);
		Unit_CheckEq(ps.y, -5);

		// Same data, minified — must produce identical result.
		constexpr char const* minified = "{\"x\":10,\"y\":-5}";
		Point pm;
		Unit_CheckRes(ToObject(testMem, testMem, minified, StrLen(minified), &pm));
		Unit_CheckEq(pm.x, 10);
		Unit_CheckEq(pm.y, -5);
	}

	Unit_SubTest("Trailing commas") {
		// Array with trailing comma
		{
			Span<I32> vals;
			Unit_CheckRes(ToArray(testMem, testMem, "[ 1, 2, 3, ]", StrLen("[ 1, 2, 3, ]"), &vals));
			Unit_CheckEq(vals.len, 3u);
			Unit_CheckEq(vals[0], 1);
			Unit_CheckEq(vals[1], 2);
			Unit_CheckEq(vals[2], 3);
		}
		// Single-element array with trailing comma
		{
			Span<I32> vals;
			Unit_CheckRes(ToArray(testMem, testMem, "[ 42, ]", StrLen("[ 42, ]"), &vals));
			Unit_CheckEq(vals.len, 1u);
			Unit_CheckEq(vals[0], 42);
		}
		// Object with trailing comma (unquoted keys)
		{
			Point p;
			Unit_CheckRes(ToObject(testMem, testMem, "{ x: 10, y: -5, }", StrLen("{ x: 10, y: -5, }"), &p));
			Unit_CheckEq(p.x, 10);
			Unit_CheckEq(p.y, -5);
		}
		// Object with trailing comma (quoted keys)
		{
			Point p;
			Unit_CheckRes(ToObject(testMem, testMem, "{ \"x\": 10, \"y\": -5, }", StrLen("{ \"x\": 10, \"y\": -5, }"), &p));
			Unit_CheckEq(p.x, 10);
			Unit_CheckEq(p.y, -5);
		}
		// Array of objects, trailing comma in both inner objects and outer array
		{
			Span<Point> pts;
			constexpr char const* json = "[ { x: 1, y: 2, }, { x: -3, y: 0, }, ]";
			Unit_CheckRes(ToArray(testMem, testMem, json, StrLen(json), &pts));
			Unit_CheckEq(pts.len, 2u);
			Unit_CheckEq(pts[0].x, 1);
			Unit_CheckEq(pts[0].y, 2);
			Unit_CheckEq(pts[1].x, -3);
			Unit_CheckEq(pts[1].y, 0);
		}
	}

	Unit_SubTest("Complex") {
		constexpr char const* json =
			"{ "
			"\"active\": true, "
			"\"version\": 3, "
			"\"offset\": -100, "
			"\"bigNum\": 9999999999, "
			"\"ratio\": 1.5, "
			"\"label\": \"hello \\\"world\\\"\", "
			"\"ids\": [ 10, 20, 30 ], "
			"\"matrix\": [ [ 1, 2, 3 ], [ 4, 5, 6 ] ], "
			"\"items\": [ "
				"{ \"name\": \"sword\",  \"count\": 2,  \"values\": [ 1.5, 2.5 ] }, "
				"{ \"name\": \"shield\", \"count\": 1,  \"values\": [ ] }, "
				"{ \"name\": \"potion\", \"count\": 10, \"values\": [ 0.25, 0.5, 0.75 ] }"
			" ] }";
		Catalog cat;
		Unit_CheckRes(ToObject(testMem, testMem, json, StrLen(json), &cat));
		Unit_CheckEq(cat.active,  true);
		Unit_CheckEq(cat.version, 3u);
		Unit_CheckEq(cat.offset,  -100);
		Unit_CheckEq(cat.bigNum,  (I64)9999999999);
		Unit_CheckEq(cat.ratio,   1.5);
		Unit_CheckEq(cat.label,   "hello \"world\"");
		Unit_CheckSpanEq(cat.ids, Span<I32>({ 10, 20, 30 }));
		Unit_CheckEq(cat.matrix.len, 2u);
		Unit_CheckSpanEq(cat.matrix[0], Span<I32>({ 1, 2, 3 }));
		Unit_CheckSpanEq(cat.matrix[1], Span<I32>({ 4, 5, 6 }));
		Unit_CheckEq(cat.items.len,        3u);
		Unit_CheckEq(cat.items[0].name,    "sword");
		Unit_CheckEq(cat.items[0].count,   2);
		Unit_CheckSpanEq(cat.items[0].values, Span<F32>({ 1.5f, 2.5f }));
		Unit_CheckEq(cat.items[1].name,    "shield");
		Unit_CheckEq(cat.items[1].count,   1);
		Unit_CheckEq(cat.items[1].values.len, 0u);
		Unit_CheckEq(cat.items[2].name,    "potion");
		Unit_CheckEq(cat.items[2].count,   10);
		Unit_CheckSpanEq(cat.items[2].values, Span<F32>({ 0.25f, 0.5f, 0.75f }));
	}

}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json