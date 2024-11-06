#pragma once

#include "JC/Allocator.h"
#include "JC/Array.h"
#include "JC/Err.h"
#include "JC/Fmt.h"
#include "JC/Sys.h"
#include <stdio.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

[[noreturn]] void VPanicAt(s8 file, i32 line, s8 expr, s8 fmt, Args args) {
	static bool recursive = false;
	if (recursive) {
		Sys_Abort();
	}
	recursive = true;

	char msg[1024];
	char* begin = msg;
	char* end = msg + sizeof(msg) - 2;	// room for '\n' and '\0'
	begin = Fmt(begin, end,  "***Panic***\n");
	begin = Fmt(begin, end, "{}({})\n", file, line);
	if (expr != "") {
		begin = Fmt(begin, end, "{}\n", expr);
	}
	begin = VFmt(begin, end, fmt, args);
	if (end <= msg + sizeof(msg) - 2) {
		*end++ = '\n';
		*end = '\0';	// don't increment end to preserve the size
	} else {
		msg[sizeof(msg) - 1] = 0;
	}

	fwrite(msg, (u64)(end - begin), 1, stderr);

	if (Debugger_IsPresent()) {
		Debugger_Print(msg);
		Debugger_Break();
	}

	Sys_Abort();
}
