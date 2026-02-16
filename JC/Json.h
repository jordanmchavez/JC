#pragma once

#include "JC/Common.h"

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
	Type                     type;
	U32                      size;
	U32                      arrayDepth;
	Span<const Json::Member> members;
};

struct Member {
	Str    name;
	U32    offset;
	Traits traits;
};

constexpr Traits GetJsonTraits(bool) { return Traits { .type = Type::Bool, .size = 1,           .arrayDepth = 0, .members = {}}; }
constexpr Traits GetJsonTraits(U32)  { return Traits { .type = Type::U32,  .size = 4,           .arrayDepth = 0, .members = {}}; }
constexpr Traits GetJsonTraits(U64)  { return Traits { .type = Type::U64,  .size = 8,           .arrayDepth = 0, .members = {}}; }
constexpr Traits GetJsonTraits(I32)  { return Traits { .type = Type::I32,  .size = 4,           .arrayDepth = 0, .members = {}}; }
constexpr Traits GetJsonTraits(I64)  { return Traits { .type = Type::I64,  .size = 8,           .arrayDepth = 0, .members = {}}; }
constexpr Traits GetJsonTraits(F32)  { return Traits { .type = Type::F32,  .size = 4,           .arrayDepth = 0, .members = {}}; }
constexpr Traits GetJsonTraits(F64)  { return Traits { .type = Type::F64,  .size = 8,           .arrayDepth = 0, .members = {}}; }
constexpr Traits GetJsonTraits(Str)  { return Traits { .type = Type::Str,  .size = sizeof(Str), .arrayDepth = 0, .members = {}}; }

template <class T> constexpr Traits GetJsonTraits(Span<T>) {
	Traits traits = GetJsonTraits(T());
	traits.arrayDepth++;
	return traits;
};

#define Json_Begin(CppType) \
	namespace JsonPrivate { using CurrentType = CppType; } \
	static constexpr Json::Member CppType##JsonMembersArray[] = {

#define Json_Field(jsonName, CppMember) \
	Json::Member { \
		.name   = jsonName, \
		.offset = offsetof(JsonPrivate::CurrentType, CppMember), \
		.traits = GetJsonTraits(decltype(JsonPrivate::CurrentType::CppMember)()), \
	},

#define Json_End(CppType) \
	}; \
	static constexpr Json::Traits GetJsonTraits(CppType) { \
		return Json::Traits { \
			.type       = Json::Type::Obj, \
			.size       = sizeof(CppType), \
			.arrayDepth = 0, \
			.members    = Span<const Json::Member>(CppType##JsonMembersArray, LenOf(CppType##JsonMembersArray)); \
		}; \
	}

Res<> ObjFromJson(Mem permMem, Mem tempMem, Str json, Span<const Member> members, U8* obj);

template <class T> Res<> FromJson(Mem permMem, Mem tempMem, Str json, T* obj) {
	return ObjFromJson(permMem, tempMem, json, GetJsonTraits(*obj).members, (U8*)obj);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json