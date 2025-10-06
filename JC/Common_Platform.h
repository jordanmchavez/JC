#pragma once

namespace JC {

//--------------------------------------------------------------------------------------------------

#if defined _MSC_VER
	#define Platform_Windows
	#define Compiler_Msvc
	#define IfConstEval if (__builtin_is_constant_evaluated())
#endif	// _MSC_VER

//--------------------------------------------------------------------------------------------------

}	// namespace JC