#include "JC/Json.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/UnitTest.h"
#include <math.h>

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

JC_DEF_ERR(Json, UnmatchedComment);
JC_DEF_ERR(Json, Eof);
JC_DEF_ERR(Json, BadChar);
JC_DEF_ERR(Json, BadExponent);
JC_DEF_ERR(Json, BadNumber);
JC_DEF_ERR(Json, UnmatchedStringQuote);
JC_DEF_ERR(Json, UnmatchedArrayBracket);
JC_DEF_ERR(Json, UnmatchedObjectBrace);
JC_DEF_ERR(Json, BadEscapedChar);
JC_DEF_ERR(Json, BadObj);
JC_DEF_ERR(Json, BadKey);

//--------------------------------------------------------------------------------------------------

struct Doc {
	Allocator* allocator;
	U8*        data;
	U32        size;
	U32        cap;
	Elem       root;
};

static U8* Extend(Doc* doc, U32 size) {
	doc->size = (U32)Bit::AlignUp(doc->size, 8);
	const U32 newSize = doc->size + size;
	if (newSize > doc->cap) {
		const U32 newCap = Max(doc->cap * 2, newSize);
		doc->data = (U8*)doc->allocator->Realloc(doc->data, doc->cap, newCap);
		doc->cap = newCap;
	}
	U8* const ret = doc->data + doc->size;
	doc->size = newSize;
	return ret;
}

static Elem AddI64(Doc* doc, I64 val) {
	I64* const ptr = (I64*)Extend(doc, (U32)sizeof(val));
	*ptr = val;
	return Elem { .type = (U32)Type::I64, .offset = (U32)((U8*)ptr - doc->data) };
}

static Elem AddF64(Doc* doc, F64 val) {
	F64* const ptr = (F64*)Extend(doc, (U32)sizeof(val));
	*ptr = val;
	return Elem { .type = (U32)Type::F64, .offset = (U32)((U8*)ptr - doc->data) };
}

static Elem AddStr(Doc* doc, Str val) {
	U32* const ptr = (U32*)Extend(doc, (U32)(sizeof(val.len) + val.len));
	*ptr = (U32)val.len;
	memcpy(ptr + 1, val.data, val.len);
	return Elem { .type = (U32)Type::Str, .offset = (U32)((U8*)ptr - doc->data) };
}

static Elem AddArr(Doc* doc, Span<Elem> elems) {
	U32* const ptr = (U32*)Extend(doc, (U32)(sizeof(U64) + elems.len * sizeof(Elem)));
	*ptr = (U32)elems.len;
	memcpy(ptr + 1, elems.data, elems.len * sizeof(Elem));
	return Elem { .type = (U32)Type::Arr, .offset = (U32)((U8*)ptr - doc->data) };
}

static Elem AddObj(Doc* doc, Span<Elem> keys, Span<Elem> vals) {
	JC_ASSERT(keys.len == vals.len);
	U8* ptr = Extend(doc, (U32)(sizeof(U64) + (keys.len * 2 * sizeof(Elem))));
	*((U32*)ptr) = (U32)keys.len;
	memcpy(ptr + sizeof(U32), keys.data, keys.len * sizeof(Elem));
	memcpy(ptr + sizeof(U32) + (keys.len * sizeof(Elem)), vals.data, vals.len * sizeof(Elem));
	return Elem { .type = (U32)Type::Obj, .offset = (U32)((U8*)ptr - doc->data) };
}

//--------------------------------------------------------------------------------------------------

Elem GetRoot(Doc* doc) {
	return doc->root;
}

void Free(Doc* doc) {
	doc->allocator->Free(doc->data, doc->cap);
	doc->data = 0;
	doc->size = 0;
	doc->cap  = 0;
	doc->root = {};
}

Bool GetBool(const Doc*, Elem elem) {
	JC_ASSERT(elem.type == Type::Bool);
	return (Bool)elem.offset;
}

I64 GetI64(const Doc* doc, Elem elem) {
	JC_ASSERT(elem.type == Type::I64);
	return *(I64*)(doc->data + elem.offset);
}

U64 GetU64(const Doc* doc, Elem elem) {
	JC_ASSERT(elem.type == Type::I64);	// We store U64 as i64
	return *(U64*)(doc->data + elem.offset);
}

F64 GetF64(const Doc* doc, Elem elem) {
	JC_ASSERT(elem.type == Type::F64);
	return *(F64*)(doc->data + elem.offset);
}

Str GetStr(const Doc* doc, Elem elem) {
	JC_ASSERT(elem.type == Type::Str);
	const U32* ptr = (U32*)(doc->data + elem.offset);
	return Str((const char*)(ptr + 1), (U64)*ptr);
}

Span<Elem> GetArr(const Doc* doc, Elem elem) {
	JC_ASSERT(elem.type == Type::Arr);
	const U32* ptr = (U32*)(doc->data + elem.offset);
	return Span<Elem>((Elem*)(ptr + 1), (U64)*ptr);
}

Obj GetObj(const Doc* doc, Elem elem) {
	JC_ASSERT(elem.type == Type::Obj);
	const U8* ptr = doc->data + elem.offset;
	const U64 len = (U64)(*((U32*)ptr));
	ptr += sizeof(U32);
	Elem* keys = (Elem*)ptr;
	ptr += len * sizeof(Elem);
	Elem* vals = (Elem*)ptr;
	return Obj { .keys = Span<Elem>(keys, len), .vals = Span<Elem>(vals, len) };
}

//--------------------------------------------------------------------------------------------------

struct ParseCtx {
	Doc*           doc           = 0;
	TempAllocator* tempAllocator = 0;
	const char*    iter          = 0;
	const char*    end           = 0;
	U32            line          = 0;
};

static constexpr Bool IsSpace(char c) { return c <= 32; }

static Res<> SkipWhitespace(ParseCtx* ctx) {
	while (ctx->iter < ctx->end) {
		const char c = *ctx->iter;
		if (c == '\n') {
			ctx->line++;
			ctx->iter++;
		} else if (IsSpace(c)) {
			ctx->iter++;
		} else if (c == '/' && ctx->iter + 1 < ctx->end) {
			if (ctx->iter[1] == '/') {
				ctx->iter += 2;
				while (ctx->iter < ctx->end && *ctx->iter != '\n') {
					ctx->iter++;
				}
				ctx->iter++;	// okay if we go past end
				ctx->line++;

			} else if (ctx->iter[1] == '*') {
				ctx->iter += 2;
				const U32 commentStartLine = ctx->line;
				for (;;) {
					if (ctx->iter >= ctx->end) {
						return Err_UnmatchedComment("line", commentStartLine);
					} else if (ctx->iter + 1 < ctx->end && ctx->iter[0] == '*' && ctx->iter[1] == '/') {
						ctx->iter += 2;
						break;
					} else if (ctx->iter[0] == '\n') {
						ctx->line++;
					}
				}
			}
		} else {
			break;
		}
	}
	return Ok();
}

static Res<> Expect(ParseCtx* ctx, char c) {
	if (ctx->iter >= ctx->end) {
		return Err_Eof("line", ctx->line, "expected", c);
	}
	if (ctx->iter[0] != c) {
		return Err_BadChar("line", ctx->line, "expected", c, "actual", ctx->iter[0]);
	}
	ctx->iter++;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseTrue(ParseCtx* ctx) {
	if (Res<> r = Expect(ctx, 't'); !r) { return r.err; }
	if (Res<> r = Expect(ctx, 'r'); !r) { return r.err; }
	if (Res<> r = Expect(ctx, 'u'); !r) { return r.err; }
	if (Res<> r = Expect(ctx, 'e'); !r) { return r.err; }
	return Elem { .type = Type::Bool, .offset = 1 };
}

static Res<Elem> ParseFalse(ParseCtx* ctx) {
	if (Res<> r = Expect(ctx, 'f'); !r) { return r.err; }
	if (Res<> r = Expect(ctx, 'a'); !r) { return r.err; }
	if (Res<> r = Expect(ctx, 'l'); !r) { return r.err; }
	if (Res<> r = Expect(ctx, 's'); !r) { return r.err; }
	if (Res<> r = Expect(ctx, 'e'); !r) { return r.err; }
	return Elem { .type = Type::Bool, .offset = 0 };
}

//--------------------------------------------------------------------------------------------------

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
}

//--------------------------------------------------------------------------------------------------

static Res<Str> ParseStrRaw(ParseCtx* ctx) {
	const U32 openLine = ctx->line;
	if (Res<> r = Expect(ctx, '"'); !r) { return r.err; }
	Array<char> a(ctx->tempAllocator);
	for (;;) {
		if (ctx->iter >= ctx->end) { return Err_UnmatchedStringQuote("line", openLine); }
		if (*ctx->iter == '"') { break; }
		if (*ctx->iter == '\\') {
			ctx->iter++;
			if (ctx->iter >= ctx->end) { return Err_BadEscapedChar("line", ctx->line); }
			switch (*ctx->iter) {
				case '\\': a.Add('\\'); break;
				case '"':  a.Add('"');  break;
				case '/':  a.Add('/');  break;
				case 'b':  a.Add('\b'); break;
				case 'f':  a.Add('\f'); break;
				case 'n':  a.Add('\n'); break;
				case 'r':  a.Add('\r'); break;
				case 't':  a.Add('\t'); break;

				case 'u': // TODO: UTF-8 support

				default:
					return Err_BadChar("line", ctx->line, "ch", *ctx->iter);
			}
			ctx->iter++;

		} else {
			a.Add(*ctx->iter);
			ctx->iter++;
		}
	}

	JC_ASSERT(ctx->iter < ctx->end && *ctx->iter == '"');
	ctx->iter++;
	return Str(a.data, a.len);
}

static Res<Elem> ParseStr(ParseCtx* ctx) {
	Str s = {};
	if (Res<> r = ParseStrRaw(ctx).To(s); !r) { return r.err; }
	return AddStr(ctx->doc, s);
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseElem(ParseCtx* ctx);

static Res<Elem> ParseArr(ParseCtx* ctx) {
	const U32 openLine = ctx->line;
	if (Res<> r = Expect(ctx, '['); !r) { return r.err; }

	Array<Elem> elems(ctx->tempAllocator);
	for (;;) {
		if (Res<> r = SkipWhitespace(ctx); !r) { return r.err; }
		if (ctx->iter >= ctx->end) { return Err_UnmatchedArrayBracket("line", openLine); }
		if (*ctx->iter == ']') { break; }

		Elem e = {};
		if (Res<> r = ParseElem(ctx).To(e); !r) { return r.err; }
		elems.Add(e);
		
		if (Res<> r = SkipWhitespace(ctx); !r) { return r.err; }
		if (ctx->iter >= ctx->end) { return Err_UnmatchedArrayBracket("line", openLine); }
		if (*ctx->iter == ']') { break; }
		if (Res<> r = Expect(ctx, ','); !r) { return r.err; }
	}

	JC_ASSERT(ctx->iter < ctx->end && *ctx->iter == ']');
	ctx->iter++;
	if (Res<> r = SkipWhitespace(ctx); !r) { return r.err; }
	if (ctx->iter < ctx->end && *ctx->iter == ',') { ctx->iter++; }
	return AddArr(ctx->doc, elems);
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseKey(ParseCtx* ctx) {
	JC_ASSERT(ctx->iter < ctx->end);
	if (char c = *ctx->iter; c == '"') {
		return ParseStr(ctx);
		
	} else if (
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c == '_')
	) {
		const char* begin = ctx->iter;
		ctx->iter++;
		for (;;) {
			if (ctx->iter >= ctx->end) { return Err_BadKey("line", ctx->line); }
			c = *ctx->iter; 
			if (
				(c >= '0' && c <= '9') ||
				(c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') ||
				(c == '_')
			) {
				ctx->iter++;
			} else {
				break;
			}
		}
		return AddStr(ctx->doc, Str(begin, ctx->iter - begin));

	} else {
		return Err_BadKey("line", ctx->line, "ch", c);
	}
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseObj(ParseCtx* ctx) {
	const U32 openLine = ctx->line;
	if (Res<> r = Expect(ctx, '{'); !r) { return r.err; }

	Array<Elem> keys(ctx->tempAllocator);
	Array<Elem> vals(ctx->tempAllocator);
	for (;;) {
		if (Res<> r = SkipWhitespace(ctx); !r) { return r.err; }
		if (ctx->iter >= ctx->end) { return Err_UnmatchedObjectBrace("line", openLine); }
		if (*ctx->iter == '}') { break; }

		Elem key;
		if (Res<> r = ParseKey(ctx).To(key); !r) { return r.err; }
		keys.Add(key);

		if (Res<> r = SkipWhitespace(ctx); !r) { return r.err; }
		if (ctx->iter >= ctx->end) { return Err_UnmatchedObjectBrace("line", openLine); }

		if (Res<> r = Expect(ctx, ':'); !r) { return r.err; }

		if (Res<> r = SkipWhitespace(ctx); !r) { return r.err; }
		if (ctx->iter >= ctx->end) { return Err_UnmatchedObjectBrace("line", openLine); }

		Elem val;
		if (Res<> r = ParseElem(ctx).To(val); !r) { return r.err; }
		vals.Add(val);
		
		if (Res<> r = SkipWhitespace(ctx); !r) { return r.err; }
		if (ctx->iter >= ctx->end) { return Err_UnmatchedObjectBrace("line", openLine); }
		if (*ctx->iter == '}') { break; }
		if (Res<> r = Expect(ctx, ','); !r) { return r.err; }
	}

	JC_ASSERT(ctx->iter < ctx->end && *ctx->iter == '}');
	ctx->iter++;
	return AddObj(ctx->doc, keys, vals);
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseElem(ParseCtx* ctx) {
	JC_ASSERT(ctx->iter < ctx->end);

	switch (ctx->iter[0]) {
		case 't': return ParseTrue(ctx);
		case 'f': return ParseFalse(ctx);

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '-': return ParseNum(ctx);

		case '"': return ParseStr(ctx);
		
		case '[': return ParseArr(ctx);

		case '{': return ParseObj(ctx);

		default: return Err_BadChar("line", ctx->line, "ch", ctx->iter[0]);
	}
}

Res<Doc*> Parse(Allocator* allocator, TempAllocator* tempAllocator, Str json) {
	ParseCtx ctx = {
		.doc           = allocator->AllocT<Doc>(),
		.tempAllocator = tempAllocator,
		.iter          = json.data,
		.end           = json.data + json.len,
		.line          = 1,
	};
	ctx.doc->allocator = allocator;

	if (Res<> r = SkipWhitespace(&ctx); !r)              { Free(ctx.doc); return r.err; }
	if (Res<> r = ParseElem(&ctx).To(ctx.doc->root); !r) { Free(ctx.doc); return r.err; };
	if (Res<> r = SkipWhitespace(&ctx); !r)              { Free(ctx.doc); return r.err; }
	if (ctx.iter < ctx.end)                              { Free(ctx.doc); return Err_BadChar("line", ctx.line, "ch", *ctx.iter); }

	return ctx.doc;
}

//--------------------------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json