#include "JC/Unicode.h"

#include "JC/Array.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

Str16z Utf8ToWtf16z(Mem::Allocator* allocator, Str s) {
	Array<wchar_t> out(arena);

	const u8* p = (const u8*)s.data;
	const u8* end = p + s.len;
	while (p < end) {
		// U+ 0000 - U+ 007f | 0xxxyyyy                            | 0x00 - 0x7f
		// U+ 0080 - U+ 07ff | 110xxxyy 10yyzzzz                   | 0xc2 - 0xdf
		// U+ 0800 - U+ ffff | 1110xxxx 10uuuuzz 10zzuuuu          | 0xe0 - 0xef
		// U+10000 - U+10fff | 11110xyy 10yyzzzz 10uuuuvv 10vvwwww | 0xf0 - 0xf7

		// surrogates
		// U+d800 | 111011011010000010000000 | ed a0 80
		// U+dbff | 111011011010111110111111 | ed af bf
		// U+dc00 | 111011011011000010000000 | ed b0 80
		// U+dfff | 111011011011111110111111 | ed bf bf

		// illegal:
		// 0x80 - 0xbf
		// 0xf8 - 0xff

		u32 cp = (u32)L'?';
		const u32 c1 = (u32)*p++;
		if (c1 <= 0x7f) {
			cp = (u32)c1;

		} else if (c1 >= 0xc2 && c1 <= 0xdf) {
			if (p > end) { out.Add(L'?'); break; }
			const u32 c2 = (u32)*p++;
			if (c2 < 0x80 || c2 > 0xbf) { out.Add(L'?'); continue; }
			cp = ((c1 & 0x1f) << 6) + (c2 & 0x3f);

		} else if (c1 <= 0xef) {
			if (p + 1 > end) { out.Add(L'?'); break; }
			const u32 c2 = (u32)*p++;
			const u32 c3 = (u32)*p++;
			if (c1 == 0xe0) {
				if (c2 < 0xa0 || c2 > 0xbf) { out.Add(L'?'); continue; }
			} else {
				if (c2 < 0x80 || c2 > 0xbf) { out.Add(L'?'); continue; }
			}
			if (c3 < 0x80 || c3 > 0xbf) { out.Add(L'?'); continue; }
			cp = ((c1 & 0xf) << 12) + ((c2 & 0x3f) << 6) + (c3 & 0x3f);

		} else if (c1 <= 0xf4) {
			if (p + 2 > end) {
				out.Add(L'?');
				break;
			}
			const u32 c2 = (u32)*p++;
			const u32 c3 = (u32)*p++;
			const u32 c4 = (u32)*p++;
			if (c1 == 0xf0) {
				if (c2 < 0x90 || c2 > 0xbf) { out.Add(L'?'); continue; }
			} else if (c1 >= 0xf1 && c1 <= 0xf3) {
				if (c2 < 0x80 || c2 > 0xbf) { out.Add(L'?'); continue; }
			} else {
				if (c2 < 0x80 || c2 > 0x8f) { out.Add(L'?'); continue; }
			}
			if (c3 < 0x80 || c3 > 0xbf) { out.Add(L'?'); continue; }
			if (c4 < 0x80 || c4 > 0xbf) { out.Add(L'?'); continue; }
			cp = ((c1 & 0x7) << 18) + ((c2 & 0x3f) << 12) + ((c3 & 0x3f) << 6) + (c4 & 0x3f);

		} else {
			out.Add(L'?');
		}

		if (cp <= 0xffff) {
			out.Add((wchar_t)cp);
		} else {
			out.Add((wchar_t)((cp - 0x10000) >> 10) + 0xd800);
			out.Add((wchar_t)((cp - 0x10000) & 0x3ff) + 0xdc00);
		}
	}

	out.Add(u'\0');
	return Str16z(out.data, out.len - 1);
}

//--------------------------------------------------------------------------------------------------

#define CheckUtf8ToWtf16z(from8, to16z) \
	{ \
		CheckTrue(Utf8ToWtf16z(testArena, from8) == to16z); \
	}

UnitTest("Utf8ToWtf16z") {
	CheckUtf8ToWtf16z("\x00", L"\x0000");
	CheckUtf8ToWtf16z("\x7f", L"\x007f");

	CheckUtf8ToWtf16z("\xc2\x80", L"\x0080");
	CheckUtf8ToWtf16z("\xc2\xbf", L"\x00bf");
	CheckUtf8ToWtf16z("\xdf\x80", L"\x07c0");
	CheckUtf8ToWtf16z("\xdf\xbf", L"\x07ff");

	CheckUtf8ToWtf16z("\xe0\xa0\x80", L"\x0800");
	CheckUtf8ToWtf16z("\xe0\xa0\xbf", L"\x083f");
	CheckUtf8ToWtf16z("\xe0\xbf\x80", L"\xfc0");
	CheckUtf8ToWtf16z("\xe0\xbf\xbf", L"\x0fff");

	CheckUtf8ToWtf16z("\xe1\x80\x80", L"\x1000");
	CheckUtf8ToWtf16z("\xe1\x80\xbf", L"\x103f");
	CheckUtf8ToWtf16z("\xe1\xbf\x80", L"\x1fc0");
	CheckUtf8ToWtf16z("\xe1\xbf\xbf", L"\x1fff");
	CheckUtf8ToWtf16z("\xef\x80\x80", L"\xf000");
	CheckUtf8ToWtf16z("\xef\x80\xbf", L"\xf03f");
	CheckUtf8ToWtf16z("\xef\xbf\x80", L"\xffc0");
	CheckUtf8ToWtf16z("\xef\xbf\xbf", L"\xffff");
	
	CheckUtf8ToWtf16z("\xf0\x90\x80\x80", L"\xd800\xdc00");
	CheckUtf8ToWtf16z("\xf0\x90\x80\xbf", L"\xd800\xdc3f");
	CheckUtf8ToWtf16z("\xf0\x90\xbf\x80", L"\xd803\xdfc0");
	CheckUtf8ToWtf16z("\xf0\x90\xbf\xbf", L"\xd803\xdfff");
	CheckUtf8ToWtf16z("\xf0\xbf\x80\x80", L"\xd8bc\xdc00");
	CheckUtf8ToWtf16z("\xf0\xbf\x80\xbf", L"\xd8bc\xdc3f");
	CheckUtf8ToWtf16z("\xf0\xbf\xbf\x80", L"\xd8bf\xdfc0");
	CheckUtf8ToWtf16z("\xf0\xbf\xbf\xbf", L"\xd8bf\xdfff");

	CheckUtf8ToWtf16z("\xf1\x80\x80\x80", L"\xd8c0\xdc00");
	CheckUtf8ToWtf16z("\xf1\x80\x80\xbf", L"\xd8c0\xdc3f");
	CheckUtf8ToWtf16z("\xf1\x80\xbf\x80", L"\xd8c3\xdfc0");
	CheckUtf8ToWtf16z("\xf1\x80\xbf\xbf", L"\xd8c3\xdfff");

	CheckUtf8ToWtf16z("\xf1\xbf\x80\x80", L"\xd9bc\xdc00");
	CheckUtf8ToWtf16z("\xf1\xbf\x80\xbf", L"\xd9bc\xdc3f");
	CheckUtf8ToWtf16z("\xf1\xbf\xbf\x80", L"\xd9bf\xdfc0");
	CheckUtf8ToWtf16z("\xf1\xbf\xbf\xbf", L"\xd9bf\xdfff");

	CheckUtf8ToWtf16z("\xf3\x80\x80\x80", L"\xdac0\xdc00");
	CheckUtf8ToWtf16z("\xf3\x80\x80\xbf", L"\xdac0\xdc3f");
	CheckUtf8ToWtf16z("\xf3\x80\xbf\x80", L"\xdac3\xdfc0");
	CheckUtf8ToWtf16z("\xf3\x80\xbf\xbf", L"\xdac3\xdfff");

	CheckUtf8ToWtf16z("\xf3\xbf\x80\x80", L"\xdbbc\xdc00");
	CheckUtf8ToWtf16z("\xf3\xbf\x80\xbf", L"\xdbbc\xdc3f");
	CheckUtf8ToWtf16z("\xf3\xbf\xbf\x80", L"\xdbbf\xdfc0");
	CheckUtf8ToWtf16z("\xf3\xbf\xbf\xbf", L"\xdbbf\xdfff");

	CheckUtf8ToWtf16z("\xf4\x80\x80\x80", L"\xdbc0\xdc00");
	CheckUtf8ToWtf16z("\xf4\x80\x80\xbf", L"\xdbc0\xdc3f");
	CheckUtf8ToWtf16z("\xf4\x80\xbf\x80", L"\xdbc3\xdfc0");
	CheckUtf8ToWtf16z("\xf4\x80\xbf\xbf", L"\xdbc3\xdfff");
	CheckUtf8ToWtf16z("\xf4\x8f\x80\x80", L"\xdbfc\xdc00");
	CheckUtf8ToWtf16z("\xf4\x8f\x80\xbf", L"\xdbfc\xdc3f");
	CheckUtf8ToWtf16z("\xf4\x8f\xbf\x80", L"\xdbff\xdfc0");
	CheckUtf8ToWtf16z("\xf4\x8f\xbf\xbf", L"\xdbff\xdfff");

	CheckUtf8ToWtf16z((const char*)u8"hello, nice to meet you abcʦϧ턖릇块𒃶𒅋🀅🚉🤸", L"hello, nice to meet you abcʦϧ턖릇块𒃶𒅋🀅🚉🤸");
}

//--------------------------------------------------------------------------------------------------

s8 Wtf16zToUtf8(Arena* arena, Str16z s) {
	Array<char> out(arena);

	const wchar_t* end = s.data + s.len;
	const wchar_t* p = s.data;
	while (p < end) {
		u32 cp;
		const u32 c = (u32)*p++;
		if (c <= 0xd7ff || c >= 0xe000) {
			cp = c;
		} else if (c <= 0xdbff && p < end && *p >= 0xdc00 && *p <= 0xdfff) {
			cp = 0x10000 + ((c - 0xd800) << 10) + (*p - 0xdc00);
			++p;
		} else {
			cp = c;
		}

		if (cp <= 0x7f) {
			out.Add((char)cp);
		} else if (cp <= 0x7ff) {
			out.Add((char)(0xc0 | (cp >> 6)));
			out.Add((char)(0x80 | (cp & 0x3f)));
		} else if (cp <= 0xffff) {
			out.Add((char)(0xe0 | (cp >> 12)));
			out.Add((char)(0x80 | (cp >> 6) & 0x3f));
			out.Add((char)(0x80 | (cp & 0x3f)));
		} else {
			out.Add((char)(0xf0 | (cp >> 18)));
			out.Add((char)(0x80 | (cp >> 12) & 0x3f));
			out.Add((char)(0x80 | (cp >> 6) & 0x3f));
			out.Add((char)(0x80 | (cp & 0x3f)));
		}
	}

	return s8(out.data, out.len);
}

// High surrogate range
	// 1101100000000000 d800
	// 1101101111111111 dbff
// Low surrogate range
	// 1101110000000000 dc00
	// 1101111111111111 dfff

UnitTest("Unicode::Wtf16zToUtf8") {
	#define CheckWtf16zToUtf8(from16z, to8) { CheckEq(Wtf16zToUtf8(testArena, from16z), to8);  }

	// General 
	CheckWtf16zToUtf8(L"", "");
	CheckWtf16zToUtf8(L"hello", "hello");
	CheckWtf16zToUtf8(L"hello\xd800\xdc00world", "hello\xf0\x90\x80\x80world");
	CheckWtf16zToUtf8(L"hello\xd800world", "hello\xed\xa0\x80world");

	// Invalid surrogate: end-of-string
	CheckWtf16zToUtf8(L"\xd800", "\xed\xa0\x80");
	CheckWtf16zToUtf8(L"\xdbff", "\xed\xaf\xbf");

	// 1 byte
	CheckWtf16zToUtf8(L"\x0000", "\x00");
	CheckWtf16zToUtf8(L"\x007f", "\x7f");

	// 2 bytes
	CheckWtf16zToUtf8(L"\x0080", "\xc2\x80");
	CheckWtf16zToUtf8(L"\x07ff", "\xdf\xbf");

	// 3 bytes + surrogate edge cases
	CheckWtf16zToUtf8(L"\x0800", "\xe0\xa0\x80");
	CheckWtf16zToUtf8(L"\xd7ff", "\xed\x9f\xbf");
	CheckWtf16zToUtf8(L"\xe000", "\xee\x80\x80");
	CheckWtf16zToUtf8(L"\xffff", "\xef\xbf\xbf");

	// 4 bytes + surrogate edge cases
	CheckWtf16zToUtf8(L"\xd800\xdc00", "\xf0\x90\x80\x80");
	CheckWtf16zToUtf8(L"\xd800\xdfff", "\xf0\x90\x8f\xbf");
	CheckWtf16zToUtf8(L"\xdbff\xdc00", "\xf4\x8f\xb0\x80");
	CheckWtf16zToUtf8(L"\xdbff\xdfff", "\xf4\x8f\xbf\xbf");

	// Invalid surrogate: bad low surrogate
	CheckWtf16zToUtf8(L"\xd800\x0000", "\xed\xa0\x80\x00");
	CheckWtf16zToUtf8(L"\xd800\xd7ff", "\xed\xa0\x80\xed\x9f\xbf");
	CheckWtf16zToUtf8(L"\xd800\xd800", "\xed\xa0\x80\xed\xa0\x80");
	CheckWtf16zToUtf8(L"\xd800\xdbff", "\xed\xa0\x80\xed\xaf\xbf");
	CheckWtf16zToUtf8(L"\xd800\xe000", "\xed\xa0\x80\xee\x80\x80");
	CheckWtf16zToUtf8(L"\xd800\xffff", "\xed\xa0\x80\xef\xbf\xbf");

	// Invalid surrogate: bad high surrogate
	CheckWtf16zToUtf8(L"\xdc00\xdc00", "\xed\xb0\x80\xed\xb0\x80");
	CheckWtf16zToUtf8(L"\xdfff\xdc00", "\xed\xbf\xbf\xed\xb0\x80");
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC