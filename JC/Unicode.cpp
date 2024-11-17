#include "JC/Unicode.h"

#include "JC/Array.h"
#include "JC/UnitTest.h"

namespace JC::Unicode {

//--------------------------------------------------------------------------------------------------

Res<s16z> Utf8ToWtf16z(Mem* perm, s8 s) {
	Array<char16_t> out(perm);

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

		u32 cp;
		const u32 c1 = (u32)*p++;
		if (c1 <= 0x7f) {
			cp = (u32)c1;

		} else if (c1 >= 0xc2 && c1 <= 0xdf) {
			if (p > end) { return MakeErr(perm, Err_Utf8MissingTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			const u32 c2 = (u32)*p++;
			if (c2 < 0x80 || c2 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			cp = ((c1 & 0x1f) << 6) + (c2 & 0x3f);

		} else if (c1 <= 0xef) {
			if (p + 1 > end) { return MakeErr(perm, Err_Utf8MissingTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			const u32 c2 = (u32)*p++;
			const u32 c3 = (u32)*p++;
			if (c1 == 0xe0) {
				if (c2 < 0xa0 || c2 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			} else {
				if (c2 < 0x80 || c2 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			}
			if (c3 < 0x80 || c3 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			cp = ((c1 & 0xf) << 12) + ((c2 & 0x3f) << 6) + (c3 & 0x3f);

		} else if (c1 <= 0xf4) {
			if (p + 2 > end) { return MakeErr(perm, Err_Utf8MissingTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			const u32 c2 = (u32)*p++;
			const u32 c3 = (u32)*p++;
			const u32 c4 = (u32)*p++;
			if (c1 == 0xf0) {
				if (c2 < 0x90 || c2 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			} else if (c1 >= 0xf1 && c1 <= 0xf3) {
				if (c2 < 0x80 || c2 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			} else {
				if (c2 < 0x80 || c2 > 0x8f) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			}
			if (c3 < 0x80 || c3 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			if (c4 < 0x80 || c4 > 0xbf) { return MakeErr(perm, Err_Utf8BadTrailingByte, "s", s, "i", p - (const u8*)s.data); }
			cp = ((c1 & 0x7) << 18) + ((c2 & 0x3f) << 12) + ((c3 & 0x3f) << 6) + (c4 & 0x3f);

		} else {
			return MakeErr(perm, Err_Utf8BadByte, "s", s, "i", p - (const u8*)s.data);
		}

		if (cp <= 0xffff) {
			out.Add((char16_t)cp);
		} else {
			out.Add((char16_t)(((cp - 0x10000) >> 10) + 0xd800));
			out.Add((char16_t)(((cp - 0x10000) & 0x3ff) + 0xdc00));
		}
	}

	out.Add(u'\0');
	return s16z(out.data, out.len - 1);
}

//--------------------------------------------------------------------------------------------------

#define CheckUtf8ToWtf16z(from8, to16z) \
	{ \
		Mem scratchCopy = scratch; \
		Res<s16z> r = Unicode::Utf8ToWtf16z(&scratchCopy, from8); \
		if (Check(r)) { \
			Check(r.val == to16z); \
		} \
	}

UnitTest("Unicode::Utf8ToWtf16z") {
	CheckUtf8ToWtf16z("\x00", u"\x0000");
	CheckUtf8ToWtf16z("\x7f", u"\x007f");

	CheckUtf8ToWtf16z("\xc2\x80", u"\x0080");
	CheckUtf8ToWtf16z("\xc2\xbf", u"\x00bf");
	CheckUtf8ToWtf16z("\xdf\x80", u"\x07c0");
	CheckUtf8ToWtf16z("\xdf\xbf", u"\x07ff");

	CheckUtf8ToWtf16z("\xe0\xa0\x80", u"\x0800");
	CheckUtf8ToWtf16z("\xe0\xa0\xbf", u"\x083f");
	CheckUtf8ToWtf16z("\xe0\xbf\x80", u"\xfc0");
	CheckUtf8ToWtf16z("\xe0\xbf\xbf", u"\x0fff");

	CheckUtf8ToWtf16z("\xe1\x80\x80", u"\x1000");
	CheckUtf8ToWtf16z("\xe1\x80\xbf", u"\x103f");
	CheckUtf8ToWtf16z("\xe1\xbf\x80", u"\x1fc0");
	CheckUtf8ToWtf16z("\xe1\xbf\xbf", u"\x1fff");
	CheckUtf8ToWtf16z("\xef\x80\x80", u"\xf000");
	CheckUtf8ToWtf16z("\xef\x80\xbf", u"\xf03f");
	CheckUtf8ToWtf16z("\xef\xbf\x80", u"\xffc0");
	CheckUtf8ToWtf16z("\xef\xbf\xbf", u"\xffff");
	
	CheckUtf8ToWtf16z("\xf0\x90\x80\x80", u"\xd800\xdc00");
	CheckUtf8ToWtf16z("\xf0\x90\x80\xbf", u"\xd800\xdc3f");
	CheckUtf8ToWtf16z("\xf0\x90\xbf\x80", u"\xd803\xdfc0");
	CheckUtf8ToWtf16z("\xf0\x90\xbf\xbf", u"\xd803\xdfff");
	CheckUtf8ToWtf16z("\xf0\xbf\x80\x80", u"\xd8bc\xdc00");
	CheckUtf8ToWtf16z("\xf0\xbf\x80\xbf", u"\xd8bc\xdc3f");
	CheckUtf8ToWtf16z("\xf0\xbf\xbf\x80", u"\xd8bf\xdfc0");
	CheckUtf8ToWtf16z("\xf0\xbf\xbf\xbf", u"\xd8bf\xdfff");

	CheckUtf8ToWtf16z("\xf1\x80\x80\x80", u"\xd8c0\xdc00");
	CheckUtf8ToWtf16z("\xf1\x80\x80\xbf", u"\xd8c0\xdc3f");
	CheckUtf8ToWtf16z("\xf1\x80\xbf\x80", u"\xd8c3\xdfc0");
	CheckUtf8ToWtf16z("\xf1\x80\xbf\xbf", u"\xd8c3\xdfff");

	CheckUtf8ToWtf16z("\xf1\xbf\x80\x80", u"\xd9bc\xdc00");
	CheckUtf8ToWtf16z("\xf1\xbf\x80\xbf", u"\xd9bc\xdc3f");
	CheckUtf8ToWtf16z("\xf1\xbf\xbf\x80", u"\xd9bf\xdfc0");
	CheckUtf8ToWtf16z("\xf1\xbf\xbf\xbf", u"\xd9bf\xdfff");

	CheckUtf8ToWtf16z("\xf3\x80\x80\x80", u"\xdac0\xdc00");
	CheckUtf8ToWtf16z("\xf3\x80\x80\xbf", u"\xdac0\xdc3f");
	CheckUtf8ToWtf16z("\xf3\x80\xbf\x80", u"\xdac3\xdfc0");
	CheckUtf8ToWtf16z("\xf3\x80\xbf\xbf", u"\xdac3\xdfff");

	CheckUtf8ToWtf16z("\xf3\xbf\x80\x80", u"\xdbbc\xdc00");
	CheckUtf8ToWtf16z("\xf3\xbf\x80\xbf", u"\xdbbc\xdc3f");
	CheckUtf8ToWtf16z("\xf3\xbf\xbf\x80", u"\xdbbf\xdfc0");
	CheckUtf8ToWtf16z("\xf3\xbf\xbf\xbf", u"\xdbbf\xdfff");

	CheckUtf8ToWtf16z("\xf4\x80\x80\x80", u"\xdbc0\xdc00");
	CheckUtf8ToWtf16z("\xf4\x80\x80\xbf", u"\xdbc0\xdc3f");
	CheckUtf8ToWtf16z("\xf4\x80\xbf\x80", u"\xdbc3\xdfc0");
	CheckUtf8ToWtf16z("\xf4\x80\xbf\xbf", u"\xdbc3\xdfff");
	CheckUtf8ToWtf16z("\xf4\x8f\x80\x80", u"\xdbfc\xdc00");
	CheckUtf8ToWtf16z("\xf4\x8f\x80\xbf", u"\xdbfc\xdc3f");
	CheckUtf8ToWtf16z("\xf4\x8f\xbf\x80", u"\xdbff\xdfc0");
	CheckUtf8ToWtf16z("\xf4\x8f\xbf\xbf", u"\xdbff\xdfff");

	CheckUtf8ToWtf16z((const char*)u8"hello, nice to meet you abcʦϧ턖릇块𒃶𒅋🀅🚉🤸", u"hello, nice to meet you abcʦϧ턖릇块𒃶𒅋🀅🚉🤸");
}

//--------------------------------------------------------------------------------------------------

s8 Unicode::Wtf16zToUtf8(Mem* mem, s16z s) {
	Array<char> out(mem);

	const char16_t* end = s.data + s.len;
	const char16_t* p = s.data;
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
	#define CheckWtf16zToUtf8(from16z, to8) { Mem scratchCopy = scratch; CheckEq(Unicode::Wtf16zToUtf8(&scratchCopy, from16z), to8);  }

	// General 
	CheckWtf16zToUtf8(u"", "");
	CheckWtf16zToUtf8(u"hello", "hello");
	CheckWtf16zToUtf8(u"hello\xd800\xdc00world", "hello\xf0\x90\x80\x80world");
	CheckWtf16zToUtf8(u"hello\xd800world", "hello\xed\xa0\x80world");

	// Invalid surrogate: end-of-string
	CheckWtf16zToUtf8(u"\xd800", "\xed\xa0\x80");
	CheckWtf16zToUtf8(u"\xdbff", "\xed\xaf\xbf");

	// 1 byte
	CheckWtf16zToUtf8(u"\x0000", "\x00");
	CheckWtf16zToUtf8(u"\x007f", "\x7f");

	// 2 bytes
	CheckWtf16zToUtf8(u"\x0080", "\xc2\x80");
	CheckWtf16zToUtf8(u"\x07ff", "\xdf\xbf");

	// 3 bytes + surrogate edge cases
	CheckWtf16zToUtf8(u"\x0800", "\xe0\xa0\x80");
	CheckWtf16zToUtf8(u"\xd7ff", "\xed\x9f\xbf");
	CheckWtf16zToUtf8(u"\xe000", "\xee\x80\x80");
	CheckWtf16zToUtf8(u"\xffff", "\xef\xbf\xbf");

	// 4 bytes + surrogate edge cases
	CheckWtf16zToUtf8(u"\xd800\xdc00", "\xf0\x90\x80\x80");
	CheckWtf16zToUtf8(u"\xd800\xdfff", "\xf0\x90\x8f\xbf");
	CheckWtf16zToUtf8(u"\xdbff\xdc00", "\xf4\x8f\xb0\x80");
	CheckWtf16zToUtf8(u"\xdbff\xdfff", "\xf4\x8f\xbf\xbf");

	// Invalid surrogate: bad low surrogate
	CheckWtf16zToUtf8(u"\xd800\x0000", "\xed\xa0\x80\x00");
	CheckWtf16zToUtf8(u"\xd800\xd7ff", "\xed\xa0\x80\xed\x9f\xbf");
	CheckWtf16zToUtf8(u"\xd800\xd800", "\xed\xa0\x80\xed\xa0\x80");
	CheckWtf16zToUtf8(u"\xd800\xdbff", "\xed\xa0\x80\xed\xaf\xbf");
	CheckWtf16zToUtf8(u"\xd800\xe000", "\xed\xa0\x80\xee\x80\x80");
	CheckWtf16zToUtf8(u"\xd800\xffff", "\xed\xa0\x80\xef\xbf\xbf");

	// Invalid surrogate: bad high surrogate
	CheckWtf16zToUtf8(u"\xdc00\xdc00", "\xed\xb0\x80\xed\xb0\x80");
	CheckWtf16zToUtf8(u"\xdfff\xdc00", "\xed\xbf\xbf\xed\xb0\x80");
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Unicode