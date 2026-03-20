#pragma once

#include "JC/Common.h"

namespace JC { template <class T, U64 MaxLen> struct Array; }

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
	U32                arrayMaxLen;
	U32                arrayDepth;
	Span<Json::Member> members;
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

template <class T, U64 MaxLen> constexpr Traits GetJsonTraits(Array<T, MaxLen>&&) {
	Traits traits = GetJsonTraits(T());
	traits.arrayDepth++;
	traits.arrayMaxLen = MaxLen;
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
			.members    = Span<JC::Json::Member const>(CppType##Json::CppType##JsonMembers, LenOf(CppType##Json::CppType##JsonMembers)), \
		}; \
	}

Res<> JsonToObjectImpl(Mem mem, char const* json, U32 jsonLen, Traits const* traits, U8* out);

template <class T> Res<> JsonToObject(Mem mem, char const* json, U32 jsonLen, T* obj) {
	Json::Traits traits = GetJsonTraits(T());	// TODO: declval
	return ToObjectImpl(mem, json, jsonLen, &traits, (U8*)obj);
}

using TraitsFn = Traits* (Str memberName);
using DataFn = Res<void*> (Str memberName, U32 len);
Res<> ToObjectFoo(Mem mem, char const* json, U32 jsonLen, TraitsFn* traitsFn, DataFn* dataFn);

struct Ctx;

Res<Ctx*> CreateCtx(Mem mem, char const* json, U32 jsonLen);
char      CurChar(Ctx* ctx);
Res<>     Expect(Ctx* ctx, char c);
bool      Maybe(Ctx* ctx, char c);
Res<Str>  ParseName(Ctx* ctx);
Res<bool> ParseBool(Ctx* ctx);
Res<U32>  ParseU32(Ctx* ctx);
Res<U64>  ParseU64(Ctx* ctx);
Res<I32>  ParseI32(Ctx* ctx);
Res<I64>  ParseI64(Ctx* ctx);
Res<F32>  ParseF32(Ctx* ctx);
Res<F64>  ParseF64(Ctx* ctx);
Res<Str>  ParseStr(Ctx* ctx);
Res<>     ParseArray(Ctx* ctx, Traits* traits, U8* out);
Res<>     ParseObject(Ctx* ctx, Traits* traits);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Json