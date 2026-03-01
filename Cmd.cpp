#include "JC/Cmd.h"

#include "JC/Hash.h"
#include "JC/Map.h"
#include "JC/StrDb.h"
#include "JC/Unit.h"

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

// TODO:
// escape chars, especially escaped quotes inside strings
// comments // and /*
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
					parseCtx.line++;
					if (parseCtx.argsLen >= MaxArgs) {
						return Err_MaxArgs("line", parseCtx.line, "pos", (U32)(argBegin - parseCtx.begin));
					}
					parseCtx.args[parseCtx.argsLen++] = Str(argBegin, (U32)(parseCtx.iter - argBegin));
					parseCtx.iter++;
					return parseCtx;
				}
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
			CmdObj** cmdObjPtr = cmdObjMap.FindOrNull(args[0]);
			if (!cmdObjPtr) {
				return Err_UnknownCmd("cmd", args[0]);
			}
			(*cmdObjPtr)->cmdFn(Span<Str>(args, (U64)parseCtx.argsLen));
		}
	} while (!parseCtx.Eof());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Unit_Test("Cmd") {
	// TODO: add unit tests
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Cmd