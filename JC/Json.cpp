#include "JC/Json.h"

#include "JC/Array.h"
#include "JC/UnitTest.h"
#include <math.h>

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, WrongType);
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

static Elem AddI64(Arena* arena, u64 val) {
	u64* ptr = (u64*)arena->Alloc(sizeof(val));
	*ptr = val;
	return Elem { .handle = ((u64)ptr) | (u64)Type::I64 };
}

static Elem AddF64(Arena* arena, f64 val) {
	f64* ptr = (f64*)arena->Alloc(sizeof(val));
	*ptr = val;
	return Elem { .handle = ((u64)ptr) | (u64)Type::F64 };
}

static Elem AddS8(Arena* arena, s8 val) {
	u64* ptr = (u64*)arena->Alloc(sizeof(val.len) + val.len);
	*ptr = val.len;
	memcpy(ptr + 1, val.data, val.len);
	return Elem { .handle = ((u64)ptr) | (u64)Type::S8 };
}

static Elem AddArr(Arena* arena, Span<Elem> elems) {
	u64* ptr = (u64*)arena->Alloc(sizeof(u64) + (elems.len * sizeof(Elem)));
	*ptr = elems.len;
	memcpy(ptr + 1, elems.data, elems.len * sizeof(Elem));
	return Elem { .handle = ((u64)ptr) | (u64)Type::Arr };
}

static Elem AddObj(Arena* arena, Span<KeyVal> keyVals) {
	u64 keysSize = 0;
	for (u64 i = 0; i < keyVals.len; i++) {
		keysSize += keyVals[i].key.len;
	}

	char* keysPtr = (char*)arena->Alloc(keysSize);
	u64* ptr = (u64*)arena->Alloc(sizeof(u64) + (keyVals.len * sizeof(KeyVal)));
	*ptr = keyVals.len;
	KeyVal* keyValsPtr = (KeyVal*)(ptr + 1);
	for (u64 i = 0; i < keyVals.len; i++) {
		char* const keyPtr = keysPtr;
		s8 key = keyVals[i].key;
		memcpy(keyPtr, key.data, key.len);
		keysPtr += key.len;
		keyValsPtr->key = s8(keyPtr, key.len);
		keyValsPtr->val  = keyVals[i].val;
		keyValsPtr++;
	}

	return Elem { .handle = ((u64)ptr) | (u64)Type::Obj };
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

static constexpr Elem TrueElem  = { .handle = (u64)Type::Bool };
static constexpr Elem FalseElem = { .handle = (u64)Type::Bool };

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

static Res<Elem> ParseTrue(ParseCtx* p) {
	if (Res<> r = Expect(p, 't'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'r'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'u'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'e'); !r) { return r.err; }
	return TrueElem;
}

static Res<Elem> ParseFalse(ParseCtx* p) {
	if (Res<> r = Expect(p, 'f'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'a'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'l'); !r) { return r.err; }
	if (Res<> r = Expect(p, 's'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'e'); !r) { return r.err; }
	return FalseElem;
}

static Res<Elem> ParseNum(Arena* arena, ParseCtx* p) {
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
		return AddI64(arena, sign * intVal);
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

	return AddF64(arena, (double)sign * ((double)intVal + ((double)fracVal / fracDenom)) * pow(10.0, expSign * (double)exp));
}

static Res<s8> ParseStrRaw(Arena* temp, ParseCtx* p) {
	const u32 openLine = p->line;
	if (Res<> r = Expect(p, '"'); !r) { return r; }
	Array<char> a(temp);

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
	return s8(a.data, a.len);
}

static Res<Elem> ParseStr(Arena* perm, Arena* temp, ParseCtx* p) {
	s8 s = {};
	if (Res<> r = ParseStrRaw(temp, p).To(s); !r) { return r; }
	return AddS8(perm, s);
}

static Res<Elem> ParseElem(Arena* perm, Arena* temp, ParseCtx* p);

static Res<Elem> ParseArr(Arena* perm, Arena* temp, ParseCtx* p) {
	const u32 openLine = p->line;
	if (Res<> r = Expect(p, '['); !r) { return r; }

	Array<Elem> elems(temp);
	for (;;) {
		if (Res<> r = SkipWhitespace(p); !r) { return r; }
		if (p->iter >= p->end) { return Err_UnmatchedArrayBracket("line", openLine); }
		if (*p->iter == ']') { break; }

		Elem e = {};
		if (Res<> r = ParseElem(perm, temp, p).To(e); !r) { return r; }
		elems.Add(e);
		
		if (Res<> r = SkipWhitespace(p); !r) { return r; }
		if (p->iter >= p->end) { return Err_UnmatchedArrayBracket("line", openLine); }
		if (*p->iter == ']') { break; }
		if (Res<> r = Expect(p, ','); !r) { return r; }
	}

	Assert(p->iter < p->end && *p->iter == ']');
	p->iter++;
	return AddArr(perm, elems);
}

static Res<s8> ParseKey(Arena* temp, ParseCtx* p) {
	Assert(p->iter < p->end);
	if (char c = *p->iter; c == '"') {
		return ParseStrRaw(temp, p);
		
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
		return s8(begin, p->iter);
	} else {
		return Err_BadKey("line", p->line, "ch", c);
	}
}

static Res<Elem> ParseObj(Arena* perm, Arena* temp, ParseCtx* p) {
	const u32 openLine = p->line;
	if (Res<> r = Expect(p, '{'); !r) { return r; }

	Array<KeyVal> keyVals(temp);
	for (;;) {
		if (Res<> r = SkipWhitespace(p); !r) { return r; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }
		if (*p->iter == '}') { break; }

		KeyVal kv = {};
		if (Res<> r = ParseKey(temp, p).To(kv.key); !r) { return r; }

		if (Res<> r = SkipWhitespace(p); !r) { return r; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }

		if (Res<> r = Expect(p, ':'); !r) { return r; }

		if (Res<> r = SkipWhitespace(p); !r) { return r; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }

		if (Res<> r = ParseElem(perm, temp, p).To(kv.val); !r) { return r; }

		keyVals.Add(kv);
		
		if (Res<> r = SkipWhitespace(p); !r) { return r; }
		if (p->iter >= p->end) { return Err_UnmatchedObjectBrace("line", openLine); }
		if (*p->iter == '}') { break; }
		if (Res<> r = Expect(p, ','); !r) { return r; }
	}

	Assert(p->iter < p->end && *p->iter == '}');
	p->iter++;
	return AddObj(perm, keyVals);
}

static Res<Elem> ParseElem(Arena* perm, Arena* temp, ParseCtx* p) {
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
		case '-': return ParseNum(perm, p);

		case '"': return ParseStr(perm, temp, p);
		
		case '[': return ParseArr(perm, temp, p);

		case '{': return ParseObj(perm, temp, p);

		default: return Err_BadChar("line", p->line, "ch", p->iter[0]);
	}
}

Res<Elem> Parse(Arena* perm, Arena* temp, s8 str) {
	ParseCtx p = {
		.iter = str.data,
		.end  = str.data + str.len,
		.line = 1,
	};

	if (Res<> r = SkipWhitespace(&p); !r) { return r.err; }
	Elem elem = {};
	if (Res<> r = ParseElem(perm, temp, &p).To(elem); !r) { return r; };
	if (Res<> r = SkipWhitespace(&p); !r) { return r.err; }
	if (p.iter < p.end) {
		return Err_BadChar("line", p.line, "ch", *p.iter);
	}

	return elem;
}

//--------------------------------------------------------------------------------------------------

Type GetType(Elem elem) {
	return (Type)(elem.handle & 0x7);
}

Res<bool> GetBool(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::Bool) { return Err_WrongType("actualtype", type); }
	if (elem.handle == TrueElem.handle) {
		return true;
	} else if (elem.handle == FalseElem.handle) {
		return false;
	} else {
		Panic("Bool elem was not TrueElem or FalseElem");
	}
}

Res<i64> GetI64(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::I64) { return Err_WrongType("actualtype", type); }
	const u64* const ptr = (u64*)(elem.handle & ~0x7);
	return (i64)*ptr;
}

Res<f64> GetF64(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::F64) { return Err_WrongType("actualtype", type); }
	const f64* const ptr = (f64*)(elem.handle & ~0x7);
	return *ptr;
}

Res<s8> GetS8(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::S8) { return Err_WrongType("actualtype", type); }
	const u64* const ptr = (u64*)(elem.handle & ~0x7);
	return s8((const char*)(ptr + 1), *ptr);
}

Res<Span<Elem>> GetArr(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::Arr) { return Err_WrongType("actualtype", type); }
	const u64* const ptr = (u64*)(elem.handle & ~0x7);
	return Span<Elem>((Elem*)(ptr + 1), *ptr);
}

Res<Span<KeyVal>> GetObj(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::Obj) { return Err_WrongType("actualtype", type); }
	const u64* const ptr = (u64*)(elem.handle & ~0x7);
	return Span<KeyVal>((KeyVal*)(ptr + 1), *ptr);
}

//--------------------------------------------------------------------------------------------------

UnitTest("Json") {
	{
		Elem e = {};
		CheckTrue(Parse(temp, temp, "123").To(e));
		CheckEq(GetType(e), Type::I64);
		i64 val = 0;
		CheckTrue(GetI64(e).To(val));
		CheckEq(val, 123);
	}

	{
		Elem e = {};
		CheckTrue(Parse(temp, temp, "{ foo: \"hello\", \"foo bar\": 1.25 }").To(e));
		Span<KeyVal> keyVals = {};
		CheckTrue(GetObj(e).To(keyVals));

		CheckEq(keyVals.len, 2);
		CheckEq(keyVals[0].key, "foo");

		s8 s = {};
		CheckTrue(GetS8(keyVals[0].val).To(s));
		CheckEq(s, "hello");

		f64 f = {};
		CheckEq(keyVals[1].key, "foo bar");
		CheckTrue(GetF64(keyVals[1].val).To(f));
		CheckEq(f, 1.25);
	}

	// TODO: more detailed tests, especially error conditions
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json