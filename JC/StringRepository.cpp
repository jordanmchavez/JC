/*#include "JC/StringRepository.h"

#include "JC/Map.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Entry {
	s8  str;
	u32 refCount = 0;
};

struct StringRepositoryImpl : StringRepository {
	Map<u64, Entry> entries;

	u64 Add(s8 str) override {
	}
	void Ref(u64 hash) override {
	}
	void Remove(u64 hash) override {
	}
	s8 Find(u64 hash) override {
	}
};

//--------------------------------------------------------------------------------------------------

struct StringRepositoryApiImpl : StringRepositoryApi {

	StringRepository* Create() override {
	}

	void Destroy(StringRepository* repo) override {
	}
};

StringRepositoryApiImpl stringRepositoryApiImpl;

StringRepositoryApi* StringRepositoryApi::Get() {
	return &stringRepositoryApiImpl
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC*/