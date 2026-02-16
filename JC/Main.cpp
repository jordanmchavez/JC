#include "JC/FS.h"
#include "JC/Log.h"
#include "JC/Json.h"
#include "JC/StrDb.h"
#include "JC/Unit.h"

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

Res<> Run() {
	Mem permMem = Mem::Create(16 * GB);
	Mem tempMem = Mem::Create(16 * GB);
	StrDb::Init();
	Log::Init(permMem, tempMem);

	Span<char> json; TryTo(FS::ReadAllZ(permMem, "Assets/test5m.json"), json);
	Span<Entry> entries; Try(Json::ToArray(permMem, tempMem, json.data, (U32)json.len, &entries));

	return Ok();
}

int main(int argc, const char* const* argv) {
	if (argc == 2 && argv[1] == Str("test")) { Unit::Run(); return 0; }
	Res<> r = Run();
	return r ? 0 : 1;
}