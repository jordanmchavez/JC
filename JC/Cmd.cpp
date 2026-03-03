#include "JC/Cmd.h"

#include "JC/Hash.h"
#include "JC/Map.h"
#include "JC/StrDb.h"
#include "JC/UnitTest.h"

namespace JC::Cmd {

//--------------------------------------------------------------------------------------------------

DefErr(Cmd, UnmatchedQuote);
DefErr(Cmd, MaxArgs);
DefErr(Cmd, UnknownCmd);

static constexpr U32 MaxCmds = 1024;
static constexpr U32 MaxArgs = 64;

struct CmdObj {
	Str    name;
	CmdFn* cmdFn;
};

static CmdObj*           cmdObjs;
static U32               cmdObjsLen;
static Map<Str, CmdObj*> cmdObjMap;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem) {
	cmdObjs = Mem::AllocT<CmdObj>(permMem, MaxCmds);
	cmdObjsLen = 0;
	cmdObjMap.Init(permMem, MaxCmds);
}

//--------------------------------------------------------------------------------------------------

void AddCmd(Str name, CmdFn* cmdFn) {
	Assert(cmdObjsLen < MaxCmds);
	cmdObjs[cmdObjsLen] = {
		.name  = StrDb::Intern(name),
		.cmdFn = cmdFn,
	};
	cmdObjMap.Put(cmdObjs[cmdObjsLen].name, &cmdObjs[cmdObjsLen]);
	cmdObjsLen++;
}

//--------------------------------------------------------------------------------------------------

struct ParseCtx {
	char const* begin = nullptr;
	char const* iter = nullptr;
	char const* end = nullptr;
	U32         line = 0;
	Str*        args;	// MaxArgs
	U32         argsLen = 0;

	bool Eof() const { return iter >= end; }
};

static Res<ParseCtx> ParseLine(ParseCtx parseCtx) {
	for (;;) {
		// Skip whitespace
		for (;;) {
			if (parseCtx.Eof()) {
				return parseCtx;
			}
			char const c = *parseCtx.iter;
			if (c == '\n') {
				parseCtx.line++;
				parseCtx.iter++;
				return parseCtx;
			}
			if (c == ' ' || c == '\t' || c == '\r') {
				parseCtx.iter++;
			} else {
				break;
			}
		}

		Assert(!parseCtx.Eof());

		// Parse quoted string
		if (*parseCtx.iter == '"') {
			parseCtx.iter++;
			char const* const argBegin = parseCtx.iter;
			for (;;) {
				if (parseCtx.iter >= parseCtx.end) {
					return Err_UnmatchedQuote("line", parseCtx.line, "pos", (U32)(argBegin - parseCtx.begin));
				}
				char const c = *parseCtx.iter;
				if (c == '"') {
					if (parseCtx.argsLen >= MaxArgs) {
						return Err_MaxArgs("line", parseCtx.line, "pos", (U32)(argBegin - parseCtx.begin));
					}
					parseCtx.args[parseCtx.argsLen++] = Str(argBegin, (U32)(parseCtx.iter - argBegin));
					parseCtx.iter++;
					break;
				}
				// Quoted strings cannot span newlines
				if (c == '\r' || c == '\n') {
					return Err_UnmatchedQuote("line", parseCtx.line, "pos", (U32)(argBegin - parseCtx.begin));
				}
				parseCtx.iter++;
			}

		// Parse unquoted string
		} else {
			char const* const argBegin = parseCtx.iter;
			for (;;) {
				Assert(!parseCtx.Eof());
				char const c = *parseCtx.iter;
				if (c == '\n') {
					if (parseCtx.argsLen >= MaxArgs) {
						return Err_MaxArgs("line", parseCtx.line, "pos", (U32)(argBegin - parseCtx.begin));
					}
					parseCtx.line++;
					parseCtx.args[parseCtx.argsLen++] = Str(argBegin, (U32)(parseCtx.iter - argBegin));
					parseCtx.iter++;
					return parseCtx;
				}
				// Intentional that quote delimits tokens: `abc"def"` produces ["abc", "def" ].
				if (c == '"' || c == ' ' || c == '\t' || c == '\r') {
					if (parseCtx.argsLen >= MaxArgs) {
						return Err_MaxArgs("line", parseCtx.line, "pos", (U32)(argBegin - parseCtx.begin));
					}
					parseCtx.args[parseCtx.argsLen++] = Str(argBegin, (U32)(parseCtx.iter - argBegin));
					if (c != '"') { parseCtx.iter++; }
					break;
				}
				parseCtx.iter++;
				if (parseCtx.Eof()) {
					if (parseCtx.argsLen >= MaxArgs) {
						return Err_MaxArgs("line", parseCtx.line, "pos", (U32)(argBegin - parseCtx.begin));
					}
					parseCtx.args[parseCtx.argsLen++] = Str(argBegin, (U32)(parseCtx.iter - argBegin));
					return parseCtx;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------

Res<> Exec(Str buf) {
	Str args[MaxArgs];

	ParseCtx parseCtx = {
		.begin   = buf.data,
		.iter    = buf.data,
		.end     = buf.data + buf.len,
		.line    = 1,
		.args    = args,
		.argsLen = 0,
	};
	do {
		parseCtx.argsLen = 0;
		TryTo(ParseLine(parseCtx), parseCtx);
		if (parseCtx.argsLen > 0) {
			CmdObj** cmdObjPtr = cmdObjMap.FindOrNull(parseCtx.args[0]);
			if (!cmdObjPtr) {
				return Err_UnknownCmd("cmd", parseCtx.args[0]);
			}
			Try((*cmdObjPtr)->cmdFn(Span<Str>(parseCtx.args, (U64)parseCtx.argsLen)));
		}
	} while (!parseCtx.Eof());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Unit_Test("Cmd") {
	static U32 callCount;
	static Str lastArgs[MaxArgs];
	static U32 lastArgsLen;
	callCount   = 0;
	lastArgsLen = 0;

	Init(testMem);

	static auto const recordFn = [](Span<Str> args) -> Res<> {
		callCount++;
		lastArgsLen = (U32)args.len;
		for (U32 i = 0; i < args.len; i++) { lastArgs[i] = args[i]; }
		return Ok();
	};
	static auto const failFn = [](Span<Str>) -> Res<> {
		return Err_UnmatchedQuote();
	};

	AddCmd("cmd",  recordFn);
	AddCmd("fail", failFn);

	Unit_SubTest("Empty input") {
		Unit_CheckRes(Exec(""));
		Unit_CheckEq(callCount, 0u);
	}

	Unit_SubTest("Whitespace only") {
		Unit_CheckRes(Exec("   \t  "));
		Unit_CheckEq(callCount, 0u);
	}

	Unit_SubTest("Newlines only") {
		Unit_CheckRes(Exec("\n\n\n"));
		Unit_CheckEq(callCount, 0u);
	}

	Unit_SubTest("Single command") {
		Unit_CheckRes(Exec("cmd"));
		Unit_CheckEq(callCount,   1u);
		Unit_CheckEq(lastArgsLen, 1u);
		Unit_CheckEq(lastArgs[0], "cmd");
	}

	Unit_SubTest("Single command with args") {
		Unit_CheckRes(Exec("cmd a b c"));
		Unit_CheckEq(callCount,   1u);
		Unit_CheckEq(lastArgsLen, 4u);
		Unit_CheckEq(lastArgs[0], "cmd");
		Unit_CheckEq(lastArgs[1], "a");
		Unit_CheckEq(lastArgs[2], "b");
		Unit_CheckEq(lastArgs[3], "c");
	}

	Unit_SubTest("Trailing whitespace") {
		Unit_CheckRes(Exec("cmd a   "));
		Unit_CheckEq(lastArgsLen, 2u);
		Unit_CheckEq(lastArgs[1], "a");
	}

	Unit_SubTest("Multiple spaces between args") {
		Unit_CheckRes(Exec("cmd  a  b"));
		Unit_CheckEq(lastArgsLen, 3u);
	}

	Unit_SubTest("Newline terminated") {
		Unit_CheckRes(Exec("cmd a\n"));
		Unit_CheckEq(lastArgsLen, 2u);
		Unit_CheckEq(lastArgs[1], "a");
	}

	Unit_SubTest("CRLF line ending") {
		Unit_CheckRes(Exec("cmd a\r\n"));
		Unit_CheckEq(lastArgsLen, 2u);
		Unit_CheckEq(lastArgs[1], "a");
	}

	Unit_SubTest("Multiple commands") {
		Unit_CheckRes(Exec("cmd\ncmd\ncmd"));
		Unit_CheckEq(callCount, 3u);
	}

	Unit_SubTest("Blank lines between commands") {
		Unit_CheckRes(Exec("cmd\n\ncmd"));
		Unit_CheckEq(callCount, 2u);
	}

	Unit_SubTest("Quoted arg") {
		Unit_CheckRes(Exec("cmd \"hello world\""));
		Unit_CheckEq(lastArgsLen, 2u);
		Unit_CheckEq(lastArgs[1], "hello world");
	}

	Unit_SubTest("Empty quoted arg") {
		Unit_CheckRes(Exec("cmd \"\""));
		Unit_CheckEq(lastArgsLen, 2u);
		Unit_CheckEq(lastArgs[1], "");
	}

	Unit_SubTest("Adjacent quote") {
		Unit_CheckRes(Exec("cmd abc\"def\""));
		Unit_CheckEq(lastArgsLen, 3u);
		Unit_CheckEq(lastArgs[1], "abc");
		Unit_CheckEq(lastArgs[2], "def");
	}

	Unit_SubTest("Exactly max args") {
		StrBuf sb(testMem);
		sb.Printf("cmd");
		for (U32 i = 0; i < MaxArgs - 1; i++) { sb.Printf(" a"); }
		Unit_CheckRes(Exec(sb.ToStr()));
		Unit_CheckEq(callCount,   1u);
		Unit_CheckEq(lastArgsLen, MaxArgs);
	}

	Unit_SubTest("Command error propagates") {
		Res<> r = Exec("fail");
		Unit_CheckFalse(r);
	}

	Unit_SubTest("Error stops further processing") {
		Res<> r = Exec("cmd\nnotacmd\ncmd");
		Unit_CheckFalse(r);
		Unit_CheckEq(callCount, 1u);
	}

	Unit_SubTest("Unknown command") {
		Res<> r = Exec("notacmd");
		Unit_CheckFalse(r);
		Unit_Check(r.err == Err_UnknownCmd);
	}

	Unit_SubTest("Unmatched quote - EOF") {
		Res<> r = Exec("cmd \"hello");
		Unit_CheckFalse(r);
		Unit_Check(r.err == Err_UnmatchedQuote);
	}

	Unit_SubTest("Unmatched quote - newline") {
		Res<> r = Exec("cmd \"hello\n");
		Unit_CheckFalse(r);
		Unit_Check(r.err == Err_UnmatchedQuote);
	}

	Unit_SubTest("Max args exceeded") {
		StrBuf sb(testMem);
		sb.Printf("cmd");
		for (U32 i = 0; i < MaxArgs; i++) { sb.Printf(" a"); }
		Res<> r = Exec(sb.ToStr());
		Unit_CheckFalse(r);
		Unit_Check(r.err == Err_MaxArgs);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Cmd