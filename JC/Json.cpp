#include "JC/Json.h"

#include "JC/Array.h"
#include "JC/UnitTest.h"
#include <math.h>

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, UnmatchedComment);
DefErr(Json, Eof);
DefErr(Json, BadChar);
DefErr(Json, BadExponent);
DefErr(Json, BadNumber);
DefErr(Json, UnmatchedStringQuote);
DefErr(Json, UnmatchedArrayBracket);
DefErr(Json, UnmatchedObjectBrace);
DefErr(Json, BadEscapedChar);
DefErr(Json, BadObj);
DefErr(Json, BadKey);

//--------------------------------------------------------------------------------------------------

struct Doc {
	Mem::Allocator* allocator;
	u8*             data;
	u32             size;
	u32             cap;
	Elem            root;
};

static u8* Extend(Doc* doc, u32 size) {
	doc->size = (doc->size + 7) & ~7;
	const u32 newSize = doc->size + size;
	if (newSize > doc->cap) {
		const u32 newCap = Max(doc->cap * 2, newSize);
		doc->data = (u8*)doc->allocator->Realloc(doc->data, doc->cap, newCap);
		doc->cap = newCap;
	}
	u8* const ret = doc->data + doc->size;
	doc->size = newSize;
	return ret;
}

static Elem AddI64(Doc* doc, i64 val) {
	i64* const ptr = (i64*)Extend(doc, (u32)sizeof(val));
	*ptr = val;
	return Elem { .type = (u32)Type::I64, .offset = (u32)((u8*)ptr - doc->data) };
}

static Elem AddF64(Doc* doc, f64 val) {
	f64* const ptr = (f64*)Extend(doc, (u32)sizeof(val));
	*ptr = val;
	return Elem { .type = (u32)Type::F64, .offset = (u32)((u8*)ptr - doc->data) };
}

static Elem AddStr(Doc* doc, Str val) {
	u32* const ptr = (u32*)Extend(doc, (u32)(sizeof(val.len) + val.len));
	*ptr = (u32)val.len;
	memcpy(ptr + 1, val.data, val.len);
	return Elem { .type = (u32)Type::Str, .offset = (u32)((u8*)ptr - doc->data) };
}

static Elem AddArr(Doc* doc, Span<Elem> elems) {
	u32* const ptr = (u32*)Extend(doc, (u32)(sizeof(u64) + elems.len * sizeof(Elem)));
	*ptr = (u32)elems.len;
	memcpy(ptr + 1, elems.data, elems.len * sizeof(Elem));
	return Elem { .type = (u32)Type::Arr, .offset = (u32)((u8*)ptr - doc->data) };
}

static Elem AddObj(Doc* doc, Span<Elem> keys, Span<Elem> vals) {
	Assert(keys.len == vals.len);
	u8* ptr = Extend(doc, (u32)(sizeof(u64) + (keys.len * 2 * sizeof(Elem))));
	*((u32*)ptr) = (u32)keys.len;
	ptr += sizeof(u32);
	memcpy(ptr, keys.data, keys.len * sizeof(Elem));
	ptr += keys.len * sizeof(Elem);
	memcpy(ptr, vals.data, vals.len * sizeof(Elem));

	return Elem { .type = (u32)Type::Obj, .offset = (u32)((u8*)ptr - doc->data) };
}

//--------------------------------------------------------------------------------------------------

bool GetBool(const Doc*, Elem elem) {
	Assert(elem.type == Type::Bool);
	return (bool)elem.offset;
}

i64 GetI64(const Doc* doc, Elem elem) {
	Assert(elem.type == Type::I64);
	return *((i64*)doc->data + elem.offset);
}

u64 GetU64(const Doc* doc, Elem elem) {
	Assert(elem.type == Type::I64);	// intentional, this is just a simple conversion
	return *((u64*)doc->data + elem.offset);
}

f64 GetF64(const Doc* doc, Elem elem) {
	Assert(elem.type == Type::F64);
	return *((f64*)doc->data + elem.offset);
}

Str GetStr(const Doc* doc, Elem elem) {
	Assert(elem.type == Type::Str);
	const u32* ptr = (u32*)(doc->data + elem.offset);
	return Str((const char*)(ptr + 1), (u64)*ptr);
}

Span<Elem> GetArr(const Doc* doc, Elem elem) {
	Assert(elem.type == Type::Arr);
	const u32* ptr = (u32*)(doc->data + elem.offset);
	return Span<Elem>((Elem*)(ptr + 1), (u64)*ptr);
}

Obj GetObj(const Doc* doc, Elem elem) {
	Assert(elem.type == Type::Obj);
	const u8* ptr = doc->data + elem.offset;
	const u64 len = (u64)(*((u32*)ptr));
	ptr += sizeof(u32);
	const Elem* keys = (Elem*)ptr;
	ptr += len * sizeof(Elem);
	const Elem* vals = (Elem*)ptr;
	return Obj { .keys = Span<Elem>(keys, len), .vals = Span<Elem>(vals, len) };
}

//--------------------------------------------------------------------------------------------------

struct ParseCtx {
	const char* iter = 0;
	const char* end  = 0;
	u32         line = 0;
};

static constexpr bool IsSpace(char c) { return c <= 32; }

static Res<> SkipWhitespace(ParseCtx* p) {
	while (p->iter < p->end) {
		const char c = *p->iter;
		if (c == '\n') {
			p->line++;
			p->iter++;
		} else if (IsSpace(c)) {
			p->iter++;
		} else if (c == '/' && p->iter + 1 < p->end) {
			if (p->iter[1] == '/') {
				p->iter += 2;
				while (p->iter < p->end && *p->iter != '\n') {
					p->iter++;
				}
				p->iter++;	// okay if we go past end
				p->line++;

			} else if (p->iter[1] == '*') {
				p->iter += 2;
				const u32 commentStartLine = p->line;
				for (;;) {
					if (p->iter >= p->end) {
						return Err_UnmatchedComment("line", commentStartLine);
					} else if (p->iter + 1 < p->end && p->iter[0] == '*' && p->iter[1] == '/') {
						p->iter += 2;
						break;
					} else if (p->iter[0] == '\n') {
						p->line++;
					}
				}
			}
		} else {
			break;
		}
	}
	return Ok();
}

static Res<> Expect(ParseCtx* p, char c) {
	if (p->iter >= p->end) {
		return Err_Eof("line", p->line, "expected", c);
	}
	if (p->iter[0] != c) {
		return Err_BadChar("line", p->line, "expected", c, "actual", p->iter[0]);
	}
	p->iter++;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseTrue(ParseCtx* p) {
	if (Res<> r = Expect(p, 't'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'r'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'u'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'e'); !r) { return r.err; }
	return Elem { .type = Type::Bool, .offset = 1 };
}

static Res<Elem> ParseFalse(ParseCtx* p) {
	if (Res<> r = Expect(p, 'f'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'a'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'l'); !r) { return r.err; }
	if (Res<> r = Expect(p, 's'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'e'); !r) { return r.err; }
	return Elem { .type = Type::Bool, .offset = 0 };
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseNum(Doc* doc, ParseCtx* p) {
	Assert(p->iter < p->end);

	i64 sign = 1;
	if (*p->iter == '-') {
		sign = -1;
		p->iter++;
	}

	i64 intVal = 0;
	if (p->iter < p->end) {
		if (*p->iter == '0') {
			p->iter++;
		} else if (*p->iter >= '1' && *p->iter <= '9') {
			intVal = *p->iter - '0';
			p->iter++;
			while (p->iter < p->end && *p->iter >= '0' && *p->iter <= '9') {
				intVal = (intVal * 10) + (*p->iter - '0');
				p->iter++;
			}
		} else {
			return Err_BadNumber("line", p->line, "ch", *p->iter);
		}
	}

	if (p->iter >= p->end || *p->iter != '.') {
		return AddI64(doc, sign * intVal);
	}

	u64 fracVal = 0;
	f64 fracDenom = 1;
	p->iter++;
	while (p->iter < p->end && *p->iter >= '0' && *p->iter <= '9') {
		const u64 newFracVal = (fracVal * 10) + (*p->iter - '0');
		if (newFracVal < fracVal) {
			// overflow, skip remainder
			while (p->iter < p->end && *p->iter >= '0' && *p->iter <= '9') {
				p->iter++;
			}
			break;
		}
		fracVal = newFracVal;
		fracDenom *= 10;
		p->iter++;
	}

	f64 expSign = 1.0;
	u32 exp = 0;
	if (p->iter < p->end && *p->iter == 'e' || *p->iter == 'E') {
		p->iter++;
		if (p->iter >= p->end) { return Err_BadExponent("line", p->line); }
		if (*p->iter == '+') {
			p->iter++;
		} else if (*p->iter == '-') {
			expSign = -1.0;
			p->iter++;
		}
		if (p->iter >= p->end || *p->iter < '1' || *p->iter > '9') {  return Err_BadExponent("line", p->line); }
		exp = *p->iter - '0';
		p->iter++;
		while (p->iter < p->end && *p->iter >= '0' && *p->iter <= '9') {
			exp = (exp * 10) + (*p->iter - '0');
			p->iter++;
		}
	}

	return AddF64(doc, (double)sign * ((double)intVal + ((double)fracVal / fracDenom)) * pow(10.0, expSign * (double)exp));
}

//--------------------------------------------------------------------------------------------------

static Res<Str> ParseStrRaw(ParseCtx* p) {
	const u32 openLine = p->line;
	if (Res<> r = Expect(p, '"'); !r) { return r.err; }
	Array<char> a(Mem::tempAllocator);
	for (;;) {
		if (p->iter >= p->end) { return Err_UnmatchedStringQuote("line", openLine); }
		if (*p->iter == '"') { break; }
		if (*p->iter == '\\') {
			p->iter++;
			if (p->iter >= p->end) { return Err_BadEscapedChar("line", p->line); }
			switch (*p->iter) {
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
					return Err_BadChar("line", p->line, "ch", *p->iter);
			}
			p->iter++;

		} else {
			a.Add(*p->iter);
			p->iter++;
		}
	}

	Assert(p->iter < p->end && *p->iter == '"');
	p->iter++;
	return Str(a.data, a.len);
}

static Res<Elem> ParseStr(Doc* doc, ParseCtx* p) {
	Str s = {};
	if (Res<> r = ParseStrRaw(p).To(s); !r) { return r.err; }
	return AddStr(doc, s);
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseElem(Doc* doc, ParseCtx* p);

static Res<Elem> ParseArr(Doc* doc, ParseCtx* p) {
	const u32 openLine = p->line;
	if (Res<> r = Expect(p, '['); !r) { return r.err; }

	Array<Elem> elems(Mem::tempAllocator);
	for (;;) {
		if (Res<> r = SkipWhitespace(p); !r) { return r.err; }
		if (p->iter >= p->end) { return Err_UnmatchedArrayBracket("line", openLine); }
		if (*p->iter == ']') { break; }

		Elem e = {};
		if (Res<> r = ParseElem(doc, p).To(e); !r) { return r.err; }
		elems.Add(e);
		
		if (Res<> r = SkipWhitespace(p); !r) { return r.err; }
		if (p->iter >= p->end) { return Err_UnmatchedArrayBracket("line", openLine); }
		if (*p->iter == ']') { break; }
		if (Res<> r = Expect(p, ','); !r) { return r.err; }
	}

	Assert(p->iter < p->end && *p->iter == ']');
	p->iter++;
	return AddArr(doc, elems);
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseKey(Doc* doc, ParseCtx* p) {
	Assert(p->iter < p->end);
	if (char c = *p->iter; c == '"') {
		return ParseStr(doc, p);
		
	} else if (
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c == '_')
	) {
		const char* begin = p->iter;
		p->iter++;
		for (;;) {
			if (p->iter >= p->end) { return Err_BadKey("line", p->line); }
			c = *p->iter; 
			if (
				(c >= '0' && c <= '9') ||
				(c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') ||
				(c == '_')
			) {
				p->iter++;
			} else {
				break;
			}
		}
		return AddStr(doc, Str(begin, p->iter));

	} else {
		return Err_BadKey("line", p->line, "ch", c);
	}
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseObj(Doc* doc, ParseCtx* p) {
	const u32 openLine = p->line;
	if (Res<> r = Expect(p, '{'); !r) { return r.err; }

	Array<Elem> keys(Mem::tempAllocator);
	Array<Elem> vals(Mem::tempAllocator);
	for (;;) {
		if (Res<> r = SkipWhitespace(p); !r) { return r.err; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }
		if (*p->iter == '}') { break; }

		Elem key;
		if (Res<> r = ParseKey(doc, p).To(key); !r) { return r.err; }
		keys.Add(key);

		if (Res<> r = SkipWhitespace(p); !r) { return r.err; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }

		if (Res<> r = Expect(p, ':'); !r) { return r.err; }

		if (Res<> r = SkipWhitespace(p); !r) { return r.err; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }

		Elem val;
		if (Res<> r = ParseElem(doc, p).To(val); !r) { return r.err; }
		vals.Add(val);
		
		if (Res<> r = SkipWhitespace(p); !r) { return r.err; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }
		if (*p->iter == '}') { break; }
		if (Res<> r = Expect(p, ','); !r) { return r.err; }
	}

	Assert(p->iter < p->end && *p->iter == '}');
	p->iter++;
	return AddObj(doc, keys, vals);
}

//--------------------------------------------------------------------------------------------------

static Res<Elem> ParseElem(Doc* doc, ParseCtx* p) {
	Assert(p->iter < p->end);

	switch (p->iter[0]) {
		case 't': return ParseTrue(p);
		case 'f': return ParseFalse(p);

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
		case '-': return ParseNum(doc, p);

		case '"': return ParseStr(doc, p);
		
		case '[': return ParseArr(doc, p);

		case '{': return ParseObj(doc, p);

		default: return Err_BadChar("line", p->line, "ch", p->iter[0]);
	}
}

Res<Doc*> Parse(Mem::Allocator* allocator, Str json) {
	Doc* doc = allocator->AllocT<Doc>();
	ParseCtx p = {
		.iter = json.data,
		.end  = json.data + json.len,
		.line = 1,
	};

	if (Res<> r = SkipWhitespace(&p); !r) { return r.err; }
	Elem elem = {};
	if (Res<> r = ParseElem(doc, &p).To(elem); !r) { return r.err; };
	if (Res<> r = SkipWhitespace(&p); !r) { return r.err; }
	if (p.iter < p.end) {
		return Err_BadChar("line", p.line, "ch", *p.iter);
	}

	return doc;
}

//--------------------------------------------------------------------------------------------------

void Free(Doc* doc) {
	doc->allocator->Free(doc->data, doc->cap);
}

//--------------------------------------------------------------------------------------------------

UnitTest("Json") {
	{
		Doc* doc = 0;
		CheckTrue(Parse(testAllocator, "123").To(doc));
		Elem root = GetRoot(doc);
		CheckEq(GetI64(doc, root), 123);
		Free(doc);
	}

	{
		Doc* doc = 0;
		CheckTrue(Parse(testAllocator, "{ foo: \"hello\", \"foo bar\": 1.25 }").To(doc));
		Elem root = GetRoot(doc);
		Obj obj = GetObj(doc, root);

		CheckEq(obj.keys.len, 2);
		CheckEq(obj.vals.len, 2);
		CheckEq(GetStr(doc, obj.keys[0]), "foo");
		CheckEq(GetStr(doc, obj.vals[0]), "hello");
		CheckEq(GetStr(doc, obj.keys[1]), "foo bar");
		CheckEq(GetF64(doc, obj.vals[1]), 1.25);

		Free(doc);
	}

	// TODO: more detailed tests, especially error conditions
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json