#include "JC/Json.h"
#include "JC/Unit.h"

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, MaxLen);
DefErr(Json, Eof);
DefErr(Json, Unexpected);
DefErr(Json, BadChar);
DefErr(Json, MissingMember);
DefErr(Json, BadNum);

//--------------------------------------------------------------------------------------------------

static constexpr U8 Type_Null     =  0;
static constexpr U8 Type_ObjBegin =  1;
static constexpr U8 Type_ObjEnd   =  2;
static constexpr U8 Type_ArrBegin =  3;
static constexpr U8 Type_ArrEnd   =  4;
static constexpr U8 Type_Colon    =  5;
static constexpr U8 Type_Comma    =  6;
static constexpr U8 Type_Quote    =  7;
static constexpr U8 Type_Num      =  8;
static constexpr U8 Type_True     =  9;
static constexpr U8 Type_False    = 10;
static constexpr U8 Type_Eof      = 11;

static constexpr Str TypeDesc(U8 type) {
	switch (type) {
		case Type_Null:     return "null";
		case Type_ObjBegin: return "{";
		case Type_ObjEnd:   return "}";
		case Type_ArrBegin: return "[";
		case Type_ArrEnd:   return "]";
		case Type_Colon:    return ":";
		case Type_Comma:    return ",";
		case Type_Quote:    return "\"";
		case Type_Num:      return "number";
		case Type_True:     return "true";
		case Type_False:    return "false";
		case Type_Eof:      return "eof";
		default: Panic("Unhandled type: %u", type);
	}
}
//--------------------------------------------------------------------------------------------------

struct Ctx {
	U64* strMask = 0;	// 1 bit per char
	U32  strMaskLen = 0;
	U32* posType = 0;	// top 4 bits are Type_, bottom 24 are json index
	U32  posTypeLen = 0;
	Str  json;
	U32  i = 0;

	U8 Type() const {
		Assert(i < posTypeLen);
		return (U8)(posType[i] >> 24);
	}

	U32 Pos() const {
		Assert(i < posTypeLen);
		return posType[i] & 0x0fffffff;
	}

	void Advance() {
		Assert(i < posTypeLen);
		i++;
	}
};

//--------------------------------------------------------------------------------------------------

static Res<> Expect(Ctx* ctx, U8 expectedType) {
	if (ctx->Type() != expectedType) {
		return Err_Unexpected("expected", TypeDesc(expectedType));
	}
	ctx->Advance();
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Str> GetStr(Ctx* ctx) {
	Try(Expect(ctx, Type_Quote));
	const U32 startPos = ctx->Pos();
	U32 j = ctx->i;
	while (j < ctx->posTypeLen && (ctx->posType[j] >> 24) != Type_Quote) {
		j++;
	}
	const U32 endPos = ctx->posType[j] & 0x0fffffff;
	return Str(ctx->json.data + startPos, endPos - startPos);
}

//--------------------------------------------------------------------------------------------------

static Res<> InitCtx(Mem mem, Str json, Ctx* ctx) {
	if (json.len >= 256 * MB) {	// 28 bits for position in Index::typePos
		return Err_MaxLen("len", json.len);
	}

	const U64 strMaskLen = (json.len + 63) / 64;
	U64* const strMask = Mem::AllocT<U64>(mem, strMaskLen);

	// SIMD OPPORTUNITY: Replace with carry-less multiplication (simdjson §3.1.1)
	// Given positions of quotes, compute which bytes are inside strings
	bool esc = false;
	bool inStr = false;
	for (U64 i = 0; i < json.len; i++) {
		const char c = json[i];
		if (esc) {
			esc = false;
			continue;
		} else if (c == '\\' && inStr) {
			esc = true;
			continue;
		} else if (c == '"') {
			inStr = !inStr;
		}
		if (inStr) {
			strMask[i / 64] = 1ull << (i & 63);
		}
	}

	// SIMD OPPORTUNITY: Replace with SIMD scanning
	// See simdjson §3.1.2 for vectorized classification and §3.1.4 for index extraction
	U32* posType = 0;
	U32  posTypeLen = 0;
	U32  posTypeCap = 0;
	for (U64 i = 0; i < json.len; i++) {
		const char c = json[i];
		const bool insideStr = ctx->strMask[i / 64] & (1ull << (i & 63));
		if (insideStr) {
			continue;
		}

		// SIMD OPPORTUNITY: Use vpshufb-based vectorized classification (simdjson §3.1.2)
		// Can classify 32 bytes per instruction into: structural, whitespace, quote, backslash, other
		U8 type = 0;
		bool structural = false;
		switch (c) {
			case '{':  type = Type_ObjBegin; structural = true;  break;
			case '}':  type = Type_ObjEnd;   structural = true;  break;
			case '[':  type = Type_ArrBegin; structural = true;  break;
			case ']':  type = Type_ArrEnd;   structural = true;  break;
			case ':':  type = Type_Colon;    structural = true;  break;
			case ',':  type = Type_Comma;    structural = true;  break;
			case '"':  type = Type_Quote;                        break;
			case '0':  type = Type_Num;                          break;
			case '1':  type = Type_Num;                          break;
			case '2':  type = Type_Num;                          break;
			case '3':  type = Type_Num;                          break;
			case '4':  type = Type_Num;                          break;
			case '5':  type = Type_Num;                          break;
			case '6':  type = Type_Num;                          break;
			case '7':  type = Type_Num;                          break;
			case '8':  type = Type_Num;                          break;
			case '9':  type = Type_Num;                          break;
			case '-':  type = Type_Num;                          break;
			case 't':  type = Type_True;                         break;
			case 'f':  type = Type_False;                        break;
			case 'n':  type = Type_Null;                         break;
			case ' ':  continue;
			case '\t': continue;
			case '\n': continue;
			case '\r': continue;
			default: return Err_BadChar("pos", i, "char", c);
		}
		if (structural) {
			if (posTypeLen == posTypeCap) {
				posTypeCap = posTypeCap ? posTypeCap * 2 : 256;
				posType = Mem::ExtendT<U32>(mem, posType, posTypeCap);
			}
			posType[posTypeLen] = (U32)i | ((U32)type << 24u);
			posTypeLen++;
		}
	}
	if (posTypeLen == posTypeCap) {
		posTypeCap = posTypeCap ? posTypeCap * 2 : 256;
		posType = Mem::ExtendT<U32>(mem, posType, posTypeCap);
	}
	posType[posTypeLen] = (U32)json.len | ((U32)Type_Eof << 24u);
	posTypeLen++;

	ctx->strMask    = strMask;
	ctx->strMaskLen = strMaskLen;
	ctx->posType    = posType;
	ctx->posTypeLen = posTypeLen;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

/*

static Res<Elem> ParseNum(ParseCtx* ctx) {
	JC_ASSERT(ctx->iter < ctx->end);

	I64 sign = 1;
	if (*ctx->iter == '-') {
		sign = -1;
		ctx->iter++;
	}

	I64 intVal = 0;
	if (ctx->iter < ctx->end) {
		if (*ctx->iter == '0') {
			ctx->iter++;
		} else if (*ctx->iter >= '1' && *ctx->iter <= '9') {
			intVal = *ctx->iter - '0';
			ctx->iter++;
			while (ctx->iter < ctx->end && *ctx->iter >= '0' && *ctx->iter <= '9') {
				intVal = (intVal * 10) + (*ctx->iter - '0');
				ctx->iter++;
			}
		} else {
			return Err_BadNumber("line", ctx->line, "ch", *ctx->iter);
		}
	}

	if (ctx->iter >= ctx->end || *ctx->iter != '.') {
		return AddI64(ctx->doc, sign * intVal);
	}

	U64 fracVal = 0;
	F64 fracDenom = 1;
	ctx->iter++;
	while (ctx->iter < ctx->end && *ctx->iter >= '0' && *ctx->iter <= '9') {
		const U64 newFracVal = (fracVal * 10) + (*ctx->iter - '0');
		if (newFracVal < fracVal) {
			// overflow, skip remainder
			while (ctx->iter < ctx->end && *ctx->iter >= '0' && *ctx->iter <= '9') {
				ctx->iter++;
			}
			break;
		}
		fracVal = newFracVal;
		fracDenom *= 10;
		ctx->iter++;
	}

	F64 expSign = 1.0;
	U32 exp = 0;
	if (ctx->iter < ctx->end && *ctx->iter == 'e' || *ctx->iter == 'E') {
		ctx->iter++;
		if (ctx->iter >= ctx->end) { return Err_BadExponent("line", ctx->line); }
		if (*ctx->iter == '+') {
			ctx->iter++;
		} else if (*ctx->iter == '-') {
			expSign = -1.0;
			ctx->iter++;
		}
		if (ctx->iter >= ctx->end || *ctx->iter < '1' || *ctx->iter > '9') {  return Err_BadExponent("line", ctx->line); }
		exp = *ctx->iter - '0';
		ctx->iter++;
		while (ctx->iter < ctx->end && *ctx->iter >= '0' && *ctx->iter <= '9') {
			exp = (exp * 10) + (*ctx->iter - '0');
			ctx->iter++;
		}
	}

	return AddF64(ctx->doc, (double)sign * ((double)intVal + ((double)fracVal / fracDenom)) * pow(10.0, expSign * (double)exp));
}*/
static Res<F64> ParseF64(Str json, U32 pos) {
	char const*       iter = json.data + pos;
	const char* const end  = json.data + json.len;
	Assert(iter < end);
	F64 sign = 1.0;
	if (*iter == '-') {
		sign = -1.0;
		iter++;
	}
	if (iter >= end || !IsDigit(*iter)) {
		return Err_BadNum("pos", iter - json.data);
	}
	F64 val = 0;
	do {
		val = (val * 10) + (I64)(*iter - '0');
		iter++;
	} while (iter < end && IsDigit(*iter));
	return sign * val;
}

//--------------------------------------------------------------------------------------------------

static constexpr bool IsDigit(char c) { return c >= '0' && c <= '9'; }

static Res<I64> ParseI64(Str json, U32 pos) {
	char const*       iter = json.data + pos;
	const char* const end  = json.data + json.len;
	Assert(iter < end);
	I64 sign = 1;
	if (*iter == '-') {
		sign = -1;
		iter++;
	}
	if (iter >= end || !IsDigit(*iter)) {
		return Err_BadNum("pos", iter - json.data);
	}
	I64 val = 0;
	do {
		val = (val * 10) + (I64)(*iter - '0');
		iter++;
	} while (iter < end && IsDigit(*iter));
	return sign * val;
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseNum(Ctx* ctx, Type type, void* out) {
	U32 pos = ctx->Pos();
	Try(Expect(ctx, Type_Num));
	I64 ival;
	F64 fval;
	switch (type) {
		case Type::U32: TryTo(ParseI64(ctx->json, pos), ival); *(U32*)out = (U32)ival; return Ok();
	}
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseStr(Ctx* ctx, Str* out) {
	return GetStr(ctx).To(*out);
	// TODO: escape sequence;
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseVal(Mem mem, Ctx* ctx, const Traits* traits, U8* out);

static Res<> ParseArray(Mem mem, Ctx* ctx, const Traits* traits, U8* out) {
	Try(Expect(ctx, Type_ArrBegin));

	const U32 saved = ctx->i;
	U32 elemCount = 0;
	U32 depth = 0;
	for (;;) {
		const U8 type = ctx->Type();
		if (type == Type_ArrBegin || type == Type_ObjBegin) {
			depth++;
		} else if (type == Type_ArrEnd || type == Type_ObjEnd) {
			if (depth == 0) {
				break;
			}
			depth--;
		} else if (type == Type_Comma && depth == 0) {
			elemCount++;
		}
	}
	ctx->i = saved;

	Traits elemTraits = *traits;
	elemTraits.arrayDepth--;

	U8* arrayData = 0;
	if (elemCount > 0) {
		arrayData = Mem::AllocT<U8>(mem, elemCount * elemTraits.size);
	}

	for (U32 i = 0; i < elemCount; i++) {
		Try(ParseVal(mem, ctx, &elemTraits, arrayData + (i * elemTraits.size)));
		if (i < elemCount - 1) {
			Try(Expect(ctx, Type_Comma));
		}
	}

	Try(Expect(ctx, Type_ArrEnd));
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseObj(Mem mem, Ctx* ctx, Span<const Member> members, U8* out) {
	Try(Expect(ctx, Type_ObjBegin));
	for (U64 i = 0; i < members.len; i++) {
		const Member* member = &members[i];
		Str name; TryTo(GetStr(ctx), name);
		if (name != member->name) {
			return Err_Unexpected("name", member->name, "actualName", name);
		}
		Try(Expect(ctx, Type_Colon));
		Try(ParseVal(mem, ctx, &member->traits, out + member->offset));

		if (i < members.len - 1) {
			Try(Expect(ctx, Type_Comma));
		}
	}
	Try(Expect(ctx, Type_ObjEnd));
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<> ParseVal(Mem mem, Ctx* ctx, const Traits* traits, U8* out) {
	if (traits->arrayDepth > 0) {
		return ParseArray(mem, ctx, traits, out);
	}
	switch (traits->type) {
		case Type::Bool: return ParseBool(ctx, (bool*)out);
		case Type::U32:  return ParseNum(ctx, traits->type, out);
		case Type::U64:  return ParseNum(ctx, traits->type, out);
		case Type::I32:  return ParseNum(ctx, traits->type, out);
		case Type::I64:  return ParseNum(ctx, traits->type, out);
		case Type::F32:  return ParseNum(ctx, traits->type, out);
		case Type::F64:  return ParseNum(ctx, traits->type, out);
		case Type::Str:  return ParseStr(ctx, (Str*)out);
		case Type::Obj:  return ParseObj(mem, ctx, traits->members, out);
		default: Panic("Bad Json::Type: %u", (U32)traits->type);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> ObjFromJson(Mem permMem, Mem tempMem, Str json, Span<const Member> members, U8* obj) {
	U64 mark = Mem::Mark(tempMem);
	Defer { if (permMem != tempMem) { Mem::Reset(tempMem, mark); } };

	Ctx ctx = { .json = json };
	Try(InitCtx(tempMem, json, &ctx));

	const Traits traits = { .type = Type::Obj, .size = 0, .arrayDepth = 0, .members = members };
	return ParseVal(permMem, &ctx, &traits, obj);
}

//--------------------------------------------------------------------------------------------------
/*
UnitTest("Json") {
	Doc* doc = 0;
	SubTest("simple") {
		CheckTrue(Parse(testAllocator, testAllocator, "123").To(doc));
		CheckEq(GetI64(doc, GetRoot(doc)), 123);
	}

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
}
*/
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json