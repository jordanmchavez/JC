#include "JC/FS.h"
#include "JC/Log.h"
#include "JC/Json.h"
#include "JC/StrDb.h"
#include "JC/Time.h"
#include "JC/Unit.h"
#include <stdio.h>
#include "3rd/simdjson/simdjson.h"

using namespace JC;

struct Entry {
	Str name;
	Str language;
	Str id;
	Str bio;
	F64 version;
};
Json_Begin(Entry)
	Json_Member("name",     name)
	Json_Member("language", language)
	Json_Member("id",       id)
	Json_Member("bio",      bio)
	Json_Member("version",  version)
Json_End(Entry)

static Str Intern(std::string_view sv) { return StrDb::Get(Str(sv.data(), (U32)sv.size())); }

static Res<> Run() {
	Mem permMem = Mem::Create(16 * GB);
	Mem tempMem = Mem::Create(16 * GB);
	Time::Init();
	StrDb::Init();
	FS::Init(permMem, tempMem);
	Log::Init(permMem, tempMem);

	#define TEST_JC
	//#define TEST_SIMDJSON

	#if defined TEST_JC
		Span<char> json; TryTo(FS::ReadAllZ(permMem, "Assets/test5m.json"), json);
		const U64 startTicks = Time::Now();
		Span<Entry> entries; Try(Json::ToArray(permMem, tempMem, json.data, (U32)json.len, &entries));
		const U64 endTicks = Time::Now();
	printf("Time: %.2f ms\n", Time::Mils(endTicks - startTicks));
	#endif	// TEST_JC


	#if defined TEST_SIMDJSON
		char* json = 0;
		U64 jsonLen = 0;
		{
			FS::File file;
			TryTo(FS::Open("Assets/test5m.json"), file);
			Defer { FS::Close(file); };
			TryTo(FS::Len(file), jsonLen);
			json = (char*)Mem::Alloc(permMem, jsonLen + simdjson::SIMDJSON_PADDING);
			Try(FS::Read(file, json, jsonLen));
		}

		const U64 startTicks = Time::Now();
		simdjson::ondemand::parser parser;
		simdjson::ondemand::document doc;
		parser.iterate(json, jsonLen, jsonLen + simdjson::SIMDJSON_PADDING).get(doc);
		size_t entriesLen = 0;
		doc.count_elements().get(entriesLen);
		Entry* entries = Mem::AllocT<Entry>(permMem, entriesLen);
		Entry* entriesIter = entries;
		uint32_t idx = 0;
		for (auto obj : doc) {
			std::string_view name;     obj.find_field("name").get(name);
			std::string_view language; obj.find_field("language").get(language);
			std::string_view id;       obj.find_field("id").get(id);
			std::string_view bio;      obj.find_field("bio").get(bio);
			double version;            obj.find_field("version").get(version);
			*entriesIter++ = {
				.name     = Intern(name),
				.language = Intern(language),
				.id       = Intern(id),
				.bio      = Intern(bio),
				.version  = version,
			};
			entriesIter++;
		}
		const U64 endTicks = Time::Now();
		printf("Time: %.2f ms\n", Time::Mils(endTicks - startTicks));
	#endif	// TEST_SIMDJSON


	return Ok();
}

int main(int argc, const char* const* argv) {
	if (argc == 2 && argv[1] == Str("test")) { Unit::Run(); return 0; }
	Res<> r = Run();
	return r ? 0 : 1;
}