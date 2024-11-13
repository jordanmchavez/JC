#include "JC/StringRepository.h"

#include "JC/Map.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

static constexpr u32 MinChunkSize = 64 * 1024;

struct Chunk {
	u32    size = 0;
	u32    used = 0;
	Chunk* next = nullptr;
};

struct StringRepositoryImpl : StringRepository {
	Chunk* chunks = nullptr;

	u64 Add(s8 str) override {
	}

	void Ref(u64 hash) override {
	}

	void Remove(u64 hash) override {
	}

	s8 Find(u64 hash) const override {
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

}	// namespace JC