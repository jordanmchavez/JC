#include "JC/Core.h"	// not Core/Str.h to preserve core inclusion order

namespace JC {

//--------------------------------------------------------------------------------------------------

Str Copy(Mem::Allocator* allocator, Str s) {
	char* p = (char*)allocator->Alloc(s.len);
	memcpy(p, s.data, s.len);
	return Str(p, s.len);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC