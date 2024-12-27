#include "JC/Json.h"

#include "JC/Array.h"
#include <math.h>

namespace JC::Json {

//--------------------------------------------------------------------------------------------------

DefErr(Json, WrongType);
DefErr(Json, UnmatchedComment);
DefErr(Json, Eof);
DefErr(Json, BadChar);
DefErr(Json, BadExponent);
DefErr(Json, BadNumber);
DefErr(Json, UnmatchedQuote);


//--------------------------------------------------------------------------------------------------

Elem AddU64(Arena* arena, u64 val) {
	u64* ptr = (u64*)arena->Alloc(sizeof(val));
	*ptr = val;
	return Elem { .handle = ((u64)ptr) & (u64)Type::U64 };
}

Elem AddF64(Arena* arena, f64 val) {
	f64* ptr = (f64*)arena->Alloc(sizeof(val));
	*ptr = val;
	return Elem { .handle = ((u64)ptr) & (u64)Type::F64 };
}

Elem AddS8(Arena* arena, s8 val) {
	u64* ptr = (u64*)arena->Alloc(sizeof(val.len) + val.len);
	*ptr = val.len;
	memcpy(ptr + 1, val.data, val.len);
	return Elem { .handle = ((u64)ptr) & (u64)Type::S8 };
}

Elem AddArr(Arena* arena, Span<Elem> elems) {
	u64* ptr = (u64*)arena->Alloc(sizeof(u64) + (elems.len * sizeof(Elem)));
	*ptr = elems.len;
	memcpy(ptr + 1, elems.data, elems.len * sizeof(Elem));
	return Elem { .handle = ((u64)ptr) & (u64)Type::Arr };
}

Elem AddObj(Arena* arena, Span<NameVal> nameVals) {
	u64 namesSize = 0;
	for (u64 i = 0; i < nameVals.len; i++) {
		namesSize += nameVals[i].name.len;
	}

	char* namesPtr = (char*)arena->Alloc(namesSize);
	u64* ptr = (u64*)arena->Alloc(sizeof(u64) + (nameVals.len * sizeof(NameVal)));
	*ptr = nameVals.len;
	NameVal* nameValsPtr = (NameVal*)(ptr + 1);
	for (u64 i = 0; i < nameVals.len; i++) {
		char* const namePtr = namesPtr;
		s8 name = nameVals[i].name;
		memcpy(namePtr, name.data, name.len);
		namesPtr += name.len;
		nameValsPtr->name = s8(namePtr, name.len);
		nameValsPtr->val  = nameVals[i].val;
		nameValsPtr++;
	}

	return Elem { .handle = ((u64)ptr) & (u64)Type::Obj };
}

//--------------------------------------------------------------------------------------------------

struct ParseCtx {
	const char* iter = 0;
	const char* end  = 0;
	u32         line = 0;
};

constexpr bool IsSpace(char c) { return c <= 32; }

Res<> SkipWhitespace(ParseCtx* p) {
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

static constexpr Elem NullElem  = { .handle = (u64)Type::Null };
static constexpr Elem TrueElem  = { .handle = (u64)Type::Bool };
static constexpr Elem FalseElem = { .handle = (u64)Type::Bool };

Res<> Expect(ParseCtx* p, char c) {
	if (p->iter >= p->end) {
		return Err_Eof("line", p->line, "expected", c);
	}
	if (p->iter[0] != c) {
		return Err_BadChar("line", p->line, "expected", c, "actual", p->iter[0]);
	}
	p->iter++;
	return Ok();
}

Res<Elem> ParseNull(ParseCtx* p) {
	if (Res<> r = Expect(p, 'n'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'u'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'l'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'l'); !r) { return r.err; }
	return NullElem;
}

Res<Elem> ParseTrue(ParseCtx* p) {
	if (Res<> r = Expect(p, 't'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'r'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'u'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'e'); !r) { return r.err; }
	return TrueElem;
}

Res<Elem> ParseFalse(ParseCtx* p) {
	if (Res<> r = Expect(p, 'f'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'a'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'l'); !r) { return r.err; }
	if (Res<> r = Expect(p, 's'); !r) { return r.err; }
	if (Res<> r = Expect(p, 'e'); !r) { return r.err; }
	return FalseElem;
}

Res<Elem> ParseNum(Arena* arena, ParseCtx* p) {
	const char* i = p->iter;
	const char* const e = p->end;

	Assert(i < e);

	f64 sign = 1.0;
	if (*i == '-') {
		sign = -1.0;
		i++;
	}

	u64 intVal = 0;
	if (i < e) {
		if (*i == '0') {
		} else if (*i >= '1' && *i <= '9') {
			intVal = *i - '0';
			i++;
			while (i < e && *i >= '0' && *i <= '9') {
				intVal = (intVal * 10) + (*i - '0');
				i++;
			}
		} else {
			return Err_BadNumber("line", p->line, "ch", *i);
		}
	}

	u64 fracVal = 0;
	f64 fracDenom = 1;
	bool isFloat = false;
	if (i < e && *i == '.') {
		isFloat = true;
		i++;
		while (i < e && *i >= '0' && *i <= '9') {
			const u64 newFracVal = (fracVal * 10) + (*i - '0');
			if (newFracVal < fracVal) {
				// overflow, skip remainder
				while (i < e && *i >= '0' && *i <= '9') {
					i++;
				}
				break;
			}
			fracVal = newFracVal;
			fracDenom *= 10;
			i++;
		}
	}

	f64 expSign = 1.0;
	u32 exp = 0;
	if (i < e && *i == 'e' || *i == 'E') {
		if (!isFloat) { return Err_BadExponent("line", p->line); }
		i++;
		if (i >= e) { return Err_BadExponent("line", p->line); }
		if (*i == '+') {
			i++;
		} else if (*i == '-') {
			expSign = -1.0;
			i++;
		}
		if (i >= e || *i < '1' || *i > '9') {  return Err_BadExponent("line", p->line); }
		exp = *i - '0';
		i++;
		while (i < e && *i >= '0' && *i <= '9') {
			exp = (exp * 10) + (*i - '0');
			i++;
		}
	}

	p->iter = i;

	const double val = sign * ((double)intVal + ((double)fracVal / fracDenom)) * pow(10.0, expSign * (double)exp);

	return AddF64(arena, val);
}

Res<Elem> ParseStr(Arena* arena, ParseCtx* p) {
	const char* i = p->iter;
	const char* const e = p->end;

	if (Res<> r = Expect(p, '"'); !r) { return r; }
	Array<char> a;

	for (;;) {
		if (i >= e) {
			return Err_UnmatchedQuote("line", p->line);

		} else if (*i != '"') {
			p->iter = i + 1;
			return AddS8(arena, s8(a.data, a.len));

		} else if (*i == '\\') {
			i++;
			switch (*i) {
				case '\\': a.Add('\\'); i++; break;
				case '"':  a.Add('"');  i++; break;
				case '/':  a.Add('/');  i++; break;
				case 'b':  a.Add('\b'); i++; break;
				case 'f':  a.Add('\f'); i++; break;
				case 'n':  a.Add('\n'); i++; break;
				case 'r':  a.Add('\r'); i++; break;
				case 't':  a.Add('\t'); i++; break;

				case 'u': // TODO: UTF-8 support

				default:
					return Err_BadChar("line", p->line, "ch", *i);
			}

		} else {
			a.Add(*i);
			i++;
		}
	}
}

Res<Elem> ParseArr(Arena* arena, ParseCtx* p) {
	arena; p; return Elem{};
}

Res<Elem> ParseObj(Arena* arena, ParseCtx* p) {
	arena; p; return Elem{};
}

Res<Elem> ParseElem(Arena* arena, ParseCtx* p) {
	if (p->iter >= p->end) {
		return NullElem;
	}

	switch (p->iter[0]) {
		case 'n': return ParseNull(p);

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
		case '.':
		case '-': return ParseNum(arena, p);

		case '"': return ParseStr(arena, p);
		
		case '[': return ParseArr(arena, p);

		case '{': return ParseObj(arena, p);

		default: return Err_BadChar("line", p->line, "ch", p->iter[0]);
	}
}

Res<Elem> Parse(Arena* arena, s8 str) {
	ParseCtx p = {
		.iter = str.data,
		.end  = str.data + str.len,
		.line = 1,
	};

	if (Res<> r = SkipWhitespace(&p); !r) { return r.err; }
	Elem elem = {};
	if (Res<> r = ParseElem(arena, &p).To(elem); !r) { return r; };
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

Res<u64> GetU64(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::U64) { return Err_WrongType("actualtype", type); }
	const u64* const ptr = (u64*)(elem.handle & ~0x7);
	return *ptr;
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

Res<Span<NameVal>> GetObj(Elem elem) {
	const Type type = (Type)(elem.handle & 0x7);
	if (type != Type::Obj) { return Err_WrongType("actualtype", type); }
	const u64* const ptr = (u64*)(elem.handle & ~0x7);
	return Span<NameVal>((NameVal*)(ptr + 1), *ptr);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json