#include "JC/Def.h"

#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

DefErr(Def, UnregisteredName);

static constexpr U64 DefMemReserveSize = 1 * GB;
static constexpr U32 MaxRegs           = 1024;

struct RegObj {
	Reg           reg;
	Str           name;
	Json::Traits* jsonTraits;
	U8*           data;
	U32           len;
	U32           maxLen;
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
		regObj

		if (!Json::Maybe(ctx, ',')) {
			Try(Expect(ctx, '}'));
			break;
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Register(Reg reg, Str name, Json::Traits const* jsonTraits, U32 maxLen) {
}

//--------------------------------------------------------------------------------------------------

void const* GetData(Reg reg) {
}

//--------------------------------------------------------------------------------------------------

U32 GetLen(Reg reg) {
}

//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def