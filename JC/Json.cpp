// TODO: go through this and make the errors a little bit more specific, eg dont use Err_Eof for everyhing (unclosed string, unclosed array, etc)
// TODO: make this match real json5: https://spec.json5.org/

#include "JC/Json.h"

#include "JC/Array.h"
#include "JC/File.h"
#include "JC/StrDb.h"
#include "JC/UnitTest.h"
#include <math.h>	// TODO: add Pow() to math.h so we can remove this (only thing we use)

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, BadComment);
DefErr(Json, MissingClosingQuote);
DefErr(Json, Unexpected);
DefErr(Json, MissingMember);
DefErr(Json, WrongMember);
DefErr(Json, UnknownMember);
DefErr(Json, BadEscChar);
DefErr(Json, BadName);
DefErr(Json, BadBool);
DefErr(Json, BadInt);
DefErr(Json, BadFloat);
DefErr(Json, BadSci);
DefErr(Json, Eof);

//--------------------------------------------------------------------------------------------------

enum struct CharType : U8 {
	Other = 0,
	Comment,
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
		table['/' ] = CharType::Comment;
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
		for (char c = 'a'; c <= 'z'; c++) { table[(U8)c] = CharType::Alpha; }
		for (char c = 'A'; c <= 'Z'; c++) { table[(U8)c] = CharType::Alpha; }
		for (char c = '0'; c <= '9'; c++) { table[(U8)c] = CharType::Digit; }
	}
};

static constexpr CharTable charTable;

static constexpr bool IsStructural(char c) {
	CharType charType = charTable.table[(U8)c];
	return charType == CharType::Space || charType == CharType::Operator || charType == CharType::Quote;
}

static constexpr bool IsName(char c) {
	CharType charType = charTable.table[(U8)c];
	return charType == CharType::Underscore || charType == CharType::Alpha || charType == CharType::Digit;
}

static constexpr bool IsDigit(char c) {
	return charTable.table[(U8)c] == CharType::Digit;
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

	DArray<Str> elems(mem, 256);

	// TODO: Replace with SIMD scanning
	// Use vpshufb-based vectorized classification
	// Can classify 32 bytes per instruction into: structural, whitespace, quote, backslash, other
	char const* iter = ctx.data;
	while (iter < ctx.end) {
		CharType charType = charTable.table[(U8)*iter];
		switch (charType) {
			case CharType::Comment: {
				char const* comment = iter;
				iter++;
				if (iter >= ctx.end) { return Err_BadComment("pos", comment - ctx.data); }
				if (*iter == '/') {
					do {
						iter++;
					} while (iter < ctx.end && *iter != '\n');
				} else if (*iter == '*') {
					char prev = 0;
					for (;;) {
						iter++;
						if (iter >= ctx.end) { return Err_BadComment("pos", comment - ctx.data); }
						if (prev == '*' && *iter == '/') { iter++; break; }
						prev = *iter;
					}
				}
				break;
			}

			case CharType::Space:
				iter++;
				break;

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
						if (iter >= ctx.end) { return Err_BadEscChar("pos", iter - ctx.data); }
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
				} while (iter < ctx.end && !IsStructural(*iter));
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
	if (str.len == 0) { return Err_BadInt("pos", Pos(ctx, str)); }
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

// TODO: asserts or errors for range overflows (some cases in ParseF64/32 too)
static Res<I32> ParseI32(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (I32)val; }
static Res<U64> ParseU64(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (U64)val; }
static Res<U32> ParseU32(Ctx* ctx) { I64 val; TryTo(ParseI64(ctx), val); return (U32)val; }

//--------------------------------------------------------------------------------------------------

static Res<F64> ParseF64(Ctx* ctx) {
	Str str; TryTo(Read(ctx), str);
	if (str.len == 0) { return Err_BadInt("pos", Pos(ctx, str)); }
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

	MemScope(ctx->mem);
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
				default: return Err_BadEscChar("pos", Pos(ctx, str));
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

static Res<> ParseVal(Ctx* ctx, Traits const* traits, U8* out);

static Res<> ParseArray(Ctx* ctx, Traits const* traits, U8* out) {
	Try(Expect(ctx, '['));

	Str const* elemIter = ctx->elemIter;
	U64 len = 0;
	U32 depth = 0;
	for (;;) {
		if (elemIter >= ctx->elemEnd) { return Err_Eof("pos", Pos(ctx, *ctx->elemIter)); }
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
				elemIter++;	// skip the quoted string
				elemIter++;	// skip the closing quote
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
	Member const* member = members.data;
	Member const* end    = members.data + members.len;
	while (member < end) {
		if (Maybe(ctx, '}')) {
			return Ok();
		}

		Str name; TryTo(ParseName(ctx), name);
		while (member->name != name) {
			if (!member->optional) { return Err_WrongMember("expected", member->name, "actual", name); }
			member++;
			if (member >= end) { return Err_UnknownMember("name", name); }
		}
		Try(Expect(ctx, ':'));
		Try(ParseVal(ctx, member->traits, out + member->offset));

		member++;
		if (!Maybe(ctx, ',')) {
			break;
		}
	}

	while (member < end) {
		if (!member->optional) { return Err_MissingMember("name", member->name); }
		member++;
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

Res<> LoadImpl(Mem mem, Str path, Traits const* traits, U8* out) {
	ErrScope("path", path);
	Str json; TryTo(File::ReadAllStr(mem, path), json);
	return JsonToObjectImpl(mem, json, traits, out);
}

//--------------------------------------------------------------------------------------------------
// Test-only types

struct JT_Bool  { bool x; };
Json_Begin(JT_Bool)  Json_Member("x", x) Json_End(JT_Bool)

struct JT_I32   { I32  x; };
Json_Begin(JT_I32)   Json_Member("x", x) Json_End(JT_I32)

struct JT_U32   { U32  x; };
Json_Begin(JT_U32)   Json_Member("x", x) Json_End(JT_U32)

struct JT_I64   { I64  x; };
Json_Begin(JT_I64)   Json_Member("x", x) Json_End(JT_I64)

struct JT_U64   { U64  x; };
Json_Begin(JT_U64)   Json_Member("x", x) Json_End(JT_U64)

struct JT_F32   { F32  x; };
Json_Begin(JT_F32)   Json_Member("x", x) Json_End(JT_F32)

struct JT_F64   { F64  x; };
Json_Begin(JT_F64)   Json_Member("x", x) Json_End(JT_F64)

struct JT_Str   { Str  x; };
Json_Begin(JT_Str)   Json_Member("x", x) Json_End(JT_Str)

struct JT_IntArr { Span<I32> items; };
Json_Begin(JT_IntArr) Json_Member("items", items) Json_End(JT_IntArr)

struct JT_StrArr { Span<Str> items; };
Json_Begin(JT_StrArr) Json_Member("items", items) Json_End(JT_StrArr)

struct JT_Inner  { I32 y; };
Json_Begin(JT_Inner) Json_Member("y", y) Json_End(JT_Inner)

struct JT_Nested { JT_Inner inner; };
Json_Begin(JT_Nested) Json_Member("inner", inner) Json_End(JT_Nested)

struct JT_Opt { I32 req; I32 opt; };
Json_Begin(JT_Opt)
	Json_Member   ("req", req)
	Json_MemberOpt("opt", opt)
Json_End(JT_Opt)

struct JT_ComplexItem { Str name; I32 val; };
Json_Begin(JT_ComplexItem)
	Json_Member("name", name)
	Json_Member("val",  val)
Json_End(JT_ComplexItem)

struct JT_Complex {
	Str                  label;
	I32                  count;
	F64                  ratio;
	bool                 active;
	Span<I32>            ids;
	JT_ComplexItem       first;
	Span<JT_ComplexItem> items;
};
Json_Begin(JT_Complex)
	Json_Member("label",  label)
	Json_Member("count",  count)
	Json_Member("ratio",  ratio)
	Json_Member("active", active)
	Json_Member("ids",    ids)
	Json_Member("first",  first)
	Json_Member("items",  items)
Json_End(JT_Complex)

//--------------------------------------------------------------------------------------------------

Unit_Test("Json") {

	// --- Bool ---

	Unit_SubTest("Bool true") {
		JT_Bool obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: true }"), &obj));
		Unit_Check(obj.x == true);
	}

	Unit_SubTest("Bool false") {
		JT_Bool obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: false }"), &obj));
		Unit_Check(obj.x == false);
	}

	// --- Integer types ---

	Unit_SubTest("I32 positive") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 42 }"), &obj));
		Unit_CheckEq(obj.x, (I32)42);
	}

	Unit_SubTest("I32 negative") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: -7 }"), &obj));
		Unit_CheckEq(obj.x, (I32)-7);
	}

	Unit_SubTest("I32 zero") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 0 }"), &obj));
		Unit_CheckEq(obj.x, (I32)0);
	}

	Unit_SubTest("U32") {
		JT_U32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 100 }"), &obj));
		Unit_CheckEq(obj.x, (U32)100);
	}

	Unit_SubTest("I64 large positive") {
		JT_I64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 9000000000 }"), &obj));
		Unit_CheckEq(obj.x, (I64)9000000000);
	}

	Unit_SubTest("I64 large negative") {
		JT_I64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: -9000000000 }"), &obj));
		Unit_CheckEq(obj.x, (I64)-9000000000);
	}

	Unit_SubTest("U64 large") {
		JT_U64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 18000000000 }"), &obj));
		Unit_CheckEq(obj.x, (U64)18000000000);
	}

	// --- Float types ---

	Unit_SubTest("F64 integer value") {
		JT_F64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 3 }"), &obj));
		Unit_CheckEq(obj.x, 3.0);
	}

	Unit_SubTest("F64 decimal") {
		JT_F64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 1.5 }"), &obj));
		Unit_CheckEq(obj.x, 1.5);
	}

	Unit_SubTest("F64 negative decimal") {
		JT_F64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: -2.5 }"), &obj));
		Unit_CheckEq(obj.x, -2.5);
	}

	Unit_SubTest("F64 scientific e+") {
		JT_F64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 1.5e2 }"), &obj));
		Unit_CheckEq(obj.x, 150.0);
	}

	Unit_SubTest("F64 scientific e-") {
		JT_F64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 5e-1 }"), &obj));
		Unit_CheckEq(obj.x, 0.5);
	}

	Unit_SubTest("F64 scientific uppercase E") {
		JT_F64 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 1E3 }"), &obj));
		Unit_CheckEq(obj.x, 1000.0);
	}

	Unit_SubTest("F32") {
		JT_F32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 1.5 }"), &obj));
		Unit_CheckEq(obj.x, 1.5f);
	}

	// --- String ---

	Unit_SubTest("String basic") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "hello" })"), &obj));
		Unit_CheckEq(obj.x, Str("hello"));
	}

	Unit_SubTest("String empty") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)0);
	}

	Unit_SubTest("String escape newline") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\nb" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '\n');
	}

	Unit_SubTest("String escape tab") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\tb" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '\t');
	}

	Unit_SubTest("String escape double quote") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\"b" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '"');
	}

	Unit_SubTest("String escape backslash") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\\b" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '\\');
	}

	Unit_SubTest("String escape slash") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\/b" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '/');
	}

	Unit_SubTest("String escape carriage return") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\rb" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '\r');
	}

	Unit_SubTest("String escape backspace") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\bb" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '\b');
	}

	Unit_SubTest("String escape formfeed") {
		JT_Str obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ x: "a\fb" })"), &obj));
		Unit_CheckEq(obj.x.len, (U32)3);
		Unit_Check(obj.x[1] == '\f');
	}

	// --- JSON5 syntax features ---

	Unit_SubTest("Quoted key") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ "x": 5 })"), &obj));
		Unit_CheckEq(obj.x, (I32)5);
	}

	Unit_SubTest("Unquoted key") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 5 }"), &obj));
		Unit_CheckEq(obj.x, (I32)5);
	}

	Unit_SubTest("Trailing comma in object") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: 9, }"), &obj));
		Unit_CheckEq(obj.x, (I32)9);
	}

	Unit_SubTest("Trailing comma in array") {
		JT_IntArr obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ items: [1, 2, 3,] }"), &obj));
		Unit_CheckEq(obj.items.len, (U64)3);
	}

	Unit_SubTest("Single-line comment") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ // comment\nx: 5 }"), &obj));
		Unit_CheckEq(obj.x, (I32)5);
	}

	Unit_SubTest("Block comment") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ /* comment */ x: 5 }"), &obj));
		Unit_CheckEq(obj.x, (I32)5);
	}

	Unit_SubTest("Block comment spanning key and value") {
		JT_I32 obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ x: /* comment */ 7 }"), &obj));
		Unit_CheckEq(obj.x, (I32)7);
	}

	// --- Arrays ---

	Unit_SubTest("Array of ints") {
		JT_IntArr obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ items: [10, 20, 30] }"), &obj));
		Unit_CheckEq(obj.items.len, (U64)3);
		Unit_CheckEq(obj.items[0], (I32)10);
		Unit_CheckEq(obj.items[1], (I32)20);
		Unit_CheckEq(obj.items[2], (I32)30);
	}

	Unit_SubTest("Array empty") {
		JT_IntArr obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ items: [] }"), &obj));
		Unit_CheckEq(obj.items.len, (U64)0);
	}

	Unit_SubTest("Array of strings") {
		JT_StrArr obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({ items: ["foo", "bar"] })"), &obj));
		Unit_CheckEq(obj.items.len, (U64)2);
		Unit_CheckEq(obj.items[0], Str("foo"));
		Unit_CheckEq(obj.items[1], Str("bar"));
	}

	Unit_SubTest("Array single element") {
		JT_IntArr obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ items: [42] }"), &obj));
		Unit_CheckEq(obj.items.len, (U64)1);
		Unit_CheckEq(obj.items[0], (I32)42);
	}

	// --- Nested objects ---

	Unit_SubTest("Nested object") {
		JT_Nested obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ inner: { y: 7 } }"), &obj));
		Unit_CheckEq(obj.inner.y, (I32)7);
	}

	// --- Optional members ---

	Unit_SubTest("Optional member present") {
		JT_Opt obj{};
		Unit_CheckRes(JsonToObject(testMem, Str("{ req: 1, opt: 2 }"), &obj));
		Unit_CheckEq(obj.req, (I32)1);
		Unit_CheckEq(obj.opt, (I32)2);
	}

	Unit_SubTest("Optional member absent") {
		JT_Opt obj{ .opt = 99 };
		Unit_CheckRes(JsonToObject(testMem, Str("{ req: 3 }"), &obj));
		Unit_CheckEq(obj.req, (I32)3);
		Unit_CheckEq(obj.opt, (I32)99); // unchanged
	}

	// --- Complex object ---

	Unit_SubTest("Complex object") {
		JT_Complex obj{};
		Unit_CheckRes(JsonToObject(testMem, Str(R"({
			// general metadata
			label: "mission_alpha",
			count: 42,
			ratio: 3.14,  /* pi-ish */
			active: true,
			ids: [1, 2, 3, 4, 5],
			first: { name: "primary", val: 100 },
			items: [
				{ name: "a", val: 1 },
				{ name: "b", val: 2 },
			]
		})"), &obj));
		Unit_CheckEq(obj.label,        Str("mission_alpha"));
		Unit_CheckEq(obj.count,        (I32)42);
		Unit_Check(obj.ratio > 3.13 && obj.ratio < 3.15);
		Unit_Check(obj.active == true);
		Unit_CheckEq(obj.ids.len,      (U64)5);
		Unit_CheckEq(obj.ids[0],       (I32)1);
		Unit_CheckEq(obj.ids[4],       (I32)5);
		Unit_CheckEq(obj.first.val,    (I32)100);
		Unit_CheckEq(obj.first.name,   Str("primary"));
		Unit_CheckEq(obj.items.len,    (U64)2);
		Unit_CheckEq(obj.items[0].val, (I32)1);
		Unit_CheckEq(obj.items[1].val, (I32)2);
		Unit_CheckEq(obj.items[0].name, Str("a"));
		Unit_CheckEq(obj.items[1].name, Str("b"));
	}

	// --- Error: malformed inputs ---

	Unit_SubTest("Error: unclosed string") {
		JT_Str obj{};
		Unit_Check(!JsonToObject(testMem, Str(R"({ x: "hello )"), &obj));
	}

	Unit_SubTest("Error: unclosed object") {
		JT_I32 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: 1"), &obj));
	}

	Unit_SubTest("Error: unclosed array") {
		JT_IntArr obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ items: [1, 2"), &obj));
	}

	Unit_SubTest("Error: bad bool") {
		JT_Bool obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: tru }"), &obj));
	}

	Unit_SubTest("Error: bad bool garbage") {
		JT_Bool obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: maybe }"), &obj));
	}

	Unit_SubTest("Error: bad int non-digit") {
		JT_I32 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: 12abc }"), &obj));
	}

	Unit_SubTest("Error: bad int just minus") {
		JT_I32 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: - }"), &obj));
	}

	Unit_SubTest("Error: bad float trailing dot") {
		JT_F64 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: 1. }"), &obj));
	}

	Unit_SubTest("Error: bad float letter after dot") {
		JT_F64 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: 1.e5 }"), &obj));
	}

	Unit_SubTest("Error: bad sci no digit after e") {
		JT_F64 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: 1e }"), &obj));
	}

	Unit_SubTest("Error: bad escape char") {
		JT_Str obj{};
		Unit_Check(!JsonToObject(testMem, Str(R"({ x: "\x" })"), &obj));
	}

	Unit_SubTest("Error: unclosed block comment") {
		JT_I32 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ /* unclosed"), &obj));
	}

	Unit_SubTest("Error: single slash at end") {
		JT_I32 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ x: 1 }/"), &obj));
	}

	Unit_SubTest("Error: missing required member") {
		JT_I32 obj{};
		Unit_Check(!JsonToObject(testMem, Str("{}"), &obj));
	}

	Unit_SubTest("Error: wrong member order") {
		JT_Opt obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ opt: 2 }"), &obj));
	}

	Unit_SubTest("Error: unknown member") {
		JT_Opt obj{};
		Unit_Check(!JsonToObject(testMem, Str("{ req: 1, unk: 5 }"), &obj));
	}

}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json