#pragma once

#include "JC/Core.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#if !defined YESGDICAPMASKS
	#define NOGDICAPMASKS
#endif
#if !defined YESVIRTUALKEYCODES
	#define NOVIRTUALKEYCODES
#endif
#if !defined YESWINMESSAGES
	#define NOWINMESSAGES
#endif
#if !defined YESWINSTYLES
	#define NOWINSTYLES
#endif
#if !defined YESSYSMETRICS
	#define NOSYSMETRICS
#endif
#if !defined YESMENUS
	#define NOMENUS
#endif
#if !defined YESICONS
	#define NOICONS
#endif
#if !defined YESKEYSTATES
	#define NOKEYSTATES
#endif
#if !defined YESSYSCOMMANDS
	#define NOSYSCOMMANDS
#endif
#if !defined YESRASTEROPS
	#define NORASTEROPS
#endif
#if !defined YESSHOWWINDOW
	#define NOSHOWWINDOW
#endif
#if !defined YESOEMRESOURCE
	#define OEMRESOURCE
#endif
#if !defined YESATOM
	#define NOATOM
#endif
#if !defined YESCLIPBOARD
	#define NOCLIPBOARD
#endif
#if !defined YESCOLOR
	#define NOCOLOR
#endif
#if !defined YESCTLMGR
	#define NOCTLMGR
#endif
#if !defined YESDRAWTEXT
	#define NODRAWTEXT
#endif
#if !defined YESGDI
	#define NOGDI
#endif
#if !defined YESKERNEL
	#define NOKERNEL
#endif
#if !defined YESUSER
	#define NOUSER
#endif
#if !defined YESNLS
	#define NONLS
#endif
#if !defined YESMB
	#define NOMB
#endif
#if !defined YESMEMMGR
	#define NOMEMMGR
#endif
#if !defined YESMETAFILE
	#define NOMETAFILE
#endif
#if !defined YESMINMAX
	#define NOMINMAX
#endif
#if !defined YESMSG
	#define NOMSG
#endif
#if !defined YESOPENFILE
	#define NOOPENFILE
#endif
#if !defined YESSCROLL
	#define NOSCROLL
#endif
#if !defined YESSERVICE
	#define NOSERVICE
#endif
#if !defined YESSOUND
	#define NOSOUND
#endif
#if !defined YESTEXTMETRIC
	#define NOTEXTMETRIC
#endif
#if !defined YESWH
	#define NOWH
#endif
#if !defined YESWINOFFSETS
	#define NOWINOFFSETS
#endif
#if !defined YESCOMM
	#define NOCOMM
#endif
#if !defined YESKANJI
	#define NOKANJI
#endif
#if !defined YESHELP
	#define NOHELP
#endif
#if !defined YESPROFILER
	#define NOPROFILER
#endif
#if !defined YESDEFERWINDOWPOS
	#define NODEFERWINDOWPOS
#endif
#if !defined YESMCX
	#define NOMCX
#endif
#include <Windows.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class... A> struct [[nodiscard]] Err_WinLast : Err {
	static_assert(sizeof...(A) % 2 == 0);
	Err_WinLast(Str fn, A... args, SrcLoc sl = SrcLoc::Here()) {
		NamedArg namedArgs[1 + (sizeof...(A) / 2)];
		BuildNamedArgs(namedArgs, "fn", fn, args...);
		Init("Win", (i64)GetLastError(), Span<NamedArg>(namedArgs, 1 + (sizeof...(A) / 2)), sl);
	}
};
template <typename... A> Err_WinLast(Str, A...) -> Err_WinLast<A...>;

template <class... A> struct [[nodiscard]] Err_Win : JC::Err {
	static_assert(sizeof...(A) % 2 == 0);
	Err_Win(u32 code, Str fn, A... args, SrcLoc sl = SrcLoc::Here()) : Err(sl, "win", "", code, "fn", fn, args...) {}
};
template <typename... A> Err_Win(u32, Str, A...) -> Err_Win<A...>;

constexpr bool IsValidHandle(HANDLE h) {
	return h != (HANDLE)0 && h != INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC