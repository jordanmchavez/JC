#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Str {
	u64 hash;
	
};

struct StringRepository {
	virtual u64  Add(s8 str) = 0;
	virtual void Ref(u64 hash) = 0;
	virtual void Remove(u64 hash) = 0;
	virtual s8   Find(u64 hash) const = 0;

	s8 operator[](u64 hash) const { return Find(hash); }
};

struct StringRepositoryApi {
	static StringRepositoryApi* Get();

	virtual StringRepository* Create() = 0;
	virtual void              Destroy(StringRepository* repo);
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC