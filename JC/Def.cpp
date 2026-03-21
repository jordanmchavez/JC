#include "JC/Def.h"

#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"
#include "JC/StrDb.h"

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

DefErr(Def, UnregisteredName);

static constexpr U64 DefMemReserveSize = 1 * GB;
static constexpr U32 MaxRegs           = 1024;

struct RegObj {
	Str                 name;
	Reg                 reg;
	Json::Traits const* jsonTraits;
	U8*                 data;
	U64                 len;
	U64                 maxLen;
};

static Mem               tempMem;
static Mem               defMem;
static RegObj*           regObjs;
static U32               regObjsLen;
static Map<Str, RegObj*> regObjMap;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem, Mem tempMemIn) {
	tempMem    = tempMemIn;
	defMem     = Mem::Create(DefMemReserveSize);	
	regObjs    = Mem::AllocT<RegObj>(permMem, MaxRegs);
	regObjsLen = 1;	// Reserve 0 for invalid
	regObjMap.Init(permMem, MaxRegs);
}

//--------------------------------------------------------------------------------------------------

void Register(Reg reg, Str name, Json::Traits const* jsonTraits, U32 maxLen) {
	Assert(reg.handle);
	Assert(regObjMap.FindOrNull(name) == nullptr);
	Assert(regObjsLen < MaxRegs);
	RegObj* regObj = &regObjs[regObjsLen++];
	regObj->reg        = reg;
	regObj->name       = StrDb::Intern(name);
	regObj->jsonTraits = jsonTraits;
	regObj->data       = (U8*)Mem::Alloc(defMem, maxLen * jsonTraits->size);
	regObj->len        = 0;
	regObj->maxLen     = maxLen;
	regObjMap.Put(name, regObj);
}

//--------------------------------------------------------------------------------------------------

Res<> Load(Str path) {
	Span<char> jsonz; TryTo(File::ReadAllZ(path), jsonz);
	Json::Ctx* ctx; TryTo(Json::CreateCtx(defMem, jsonz.data, jsonz.len), ctx);
	
	Try(Json::Expect(ctx, '{'));
	while (Json::CurChar(ctx) != '}') {
		Str memberName; TryTo(Json::ParseName(ctx), memberName);
		Try(Expect(ctx, ':'));

		RegObj** regObjPp = regObjMap.FindOrNull(memberName);
		if (!regObjPp) {
			return Err_UnregisteredName("name", memberName);
		}

		RegObj* regObj = *regObjPp;
		Json::Traits jsonArrayTraits = {
			.type        = regObj->jsonTraits->type,
			.size        = regObj->jsonTraits->size,
			.arrayDepth  = 1,
			.arrayMaxLen = regObj->maxLen,
			.members     = {},
		};

		U8* arrayData = regObj->data;
		U64 arrayLen  = regObj->len;
		Try(Json::ParseArray(ctx, &jsonArrayTraits, arrayData, &arrayLen));
		if (!Json::Maybe(ctx, ',')) {
			Try(Expect(ctx, '}'));
			break;
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void const* GetData(Reg reg) {
	regObjMap.FindOrNull(reg.
}

//--------------------------------------------------------------------------------------------------

U32 GetLen(Reg reg) {
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def