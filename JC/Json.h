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

template<class T> constexpr Traits GetTraitsHelper(T) { return GetJsonTraits(T()); }

#define Json_Begin(CppType) \
	namespace CppType##Json { \
		using JsonType = CppType; \
		static constexpr JC::Json::Member CppType##JsonMembers[] = {

#define Json_Member(jsonName, CppMember) \
		{ \
			.name   = jsonName, \
			.offset = OffsetOf(JsonType, CppMember), \
			.traits = JC::Json::GetTraitsHelper(decltype(JsonType::CppMember)()), \
		},

#define Json_End(CppType) \
		}; \
	} \
	static constexpr JC::Json::Traits GetJsonTraits(CppType) { \
		return JC::Json::Traits { \
			.type       = JC::Json::Type::Obj, \
			.size       = sizeof(CppType), \
			.arrayDepth = 0, \
			.members    = Span<const JC::Json::Member>(CppType##Json::CppType##JsonMembers, LenOf(CppType##Json::CppType##JsonMembers)), \
		}; \
	}

Res<> ToObjectImpl(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, Span<const Member> members, U8* out);
Res<> ToArrayImpl(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, const Traits* traits, U8* out);

template <class T> Res<> ToObject(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, T* obj) {
	return ToObjectImpl(permMem, tempMem, json, jsonLen, GetJsonTraits(*obj).members, (U8*)obj);
}

template <class T> Res<> ToArray(Mem permMem, Mem tempMem, char const* json, U32 jsonLen, Span<T>* out) {
	Json::Traits traits = GetJsonTraits(T());	// TODO: declval
	traits.arrayDepth++;
	return ToArrayImpl(permMem, tempMem, json, jsonLen, &traits, (U8*)out);
}
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json