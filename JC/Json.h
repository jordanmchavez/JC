#pragma once

#include "JC/Common.h"

// Json will automatically intern all strings in the sources file
namespace JC::Json {

//--------------------------------------------------------------------------------------------------

enum struct Type {
	Invalid = 0,
	Bool,
	U32,
	U64,
	I32,
	I64,
	F32,
	F64,
	Str,
	Obj,
};

struct Member;

struct Traits {
	Type               type;
	U32                size;
	U32                arrayDepth;
	Span<Member const> members;
};

struct Member {
	Str           name;
	U32           offset;
	Traits const* traits;
};

template<class T> constexpr Traits MakeArrayTraits() {
	Traits const* traits = GetJsonTraits(T());
	return Traits {
		.type       = traits->type,
		.size       = traits->size,
		.arrayDepth = traits->arrayDepth + 1,
		.members    = {},
	};
}

constexpr Traits BoolTraits = { .type = Type::Bool, .size = 1,           .arrayDepth = 0, .members = {}};
constexpr Traits U32Traits  = { .type = Type::U32,  .size = 4,           .arrayDepth = 0, .members = {}};
constexpr Traits U64Traits  = { .type = Type::U64,  .size = 8,           .arrayDepth = 0, .members = {}};
constexpr Traits I32Traits  = { .type = Type::I32,  .size = 4,           .arrayDepth = 0, .members = {}};
constexpr Traits I64Traits  = { .type = Type::I64,  .size = 8,           .arrayDepth = 0, .members = {}};
constexpr Traits F32Traits  = { .type = Type::F32,  .size = 4,           .arrayDepth = 0, .members = {}};
constexpr Traits F64Traits  = { .type = Type::F64,  .size = 8,           .arrayDepth = 0, .members = {}};
constexpr Traits StrTraits  = { .type = Type::Str,  .size = sizeof(Str), .arrayDepth = 0, .members = {}};
template<class T> constexpr Traits ArrayTraits = MakeArrayTraits<T>();

constexpr Traits const* GetJsonTraits(bool) { return &BoolTraits; }
constexpr Traits const* GetJsonTraits(U32)  { return &U32Traits; }
constexpr Traits const* GetJsonTraits(U64)  { return &U64Traits; }
constexpr Traits const* GetJsonTraits(I32)  { return &I32Traits; }
constexpr Traits const* GetJsonTraits(I64)  { return &I64Traits; }
constexpr Traits const* GetJsonTraits(F32)  { return &F32Traits; }
constexpr Traits const* GetJsonTraits(F64)  { return &F64Traits; }
constexpr Traits const* GetJsonTraits(Str)  { return &StrTraits; }
template <class T> constexpr Traits const* GetJsonTraits(Span<T>) { return &ArrayTraits<T>; };

template<class T> constexpr Traits const* GetTraitsHelper() { return GetJsonTraits(T()); }

#define Json_Begin(CppType) \
	namespace CppType##Json { \
		using JsonType = CppType; \
		static constexpr JC::Json::Member CppType##JsonMembers[] = {

#define Json_Member(jsonName, CppMember) \
		{ \
			.name   = jsonName, \
			.offset = OffsetOf(JsonType, CppMember), \
			.traits = JC::Json::GetTraitsHelper<decltype(JsonType::CppMember)>(), \
		},

#define Json_End(CppType) \
		}; \
		static constexpr JC::Json::Traits CppType##JsonTraits = { \
			.type       = JC::Json::Type::Obj, \
			.size       = sizeof(CppType), \
			.arrayDepth = 0, \
			.members    = Span<JC::Json::Member const>(CppType##Json::CppType##JsonMembers, LenOf(CppType##Json::CppType##JsonMembers)), \
		}; \
	} \
	static constexpr JC::Json::Traits const* GetJsonTraits(CppType) { return &CppType##Json::CppType##JsonTraits; }

Res<> JsonToObjectImpl(Mem mem, Str json, Traits const* traits, U8* out);

template <class T> Res<> JsonToObject(Mem mem, Str json, U32 jsonLen, T* obj) {
	Json::Traits traits = GetJsonTraits(T());	// TODO: declval
	return ToObjectImpl(mem, json, jsonLen, &traits, (U8*)obj);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json