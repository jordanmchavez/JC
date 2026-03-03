#include "JC/Effect.h"

#include "JC/Draw.h"
#include "JC/Log.h"
#include "JC/StrDb.h"

namespace JC::Effect {

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxFloatingStrs = 256;
static constexpr F32 Z_Effect = 6.f;

struct FloatingStr {
	Draw::Font font;
	Str        str;
	F32        sec;
	F32        durSec;
	F32        t;
	F32        x;
	F32        yStart;
	F32        yEnd;
};

static FloatingStr* floatingStrs;
static U32          floatingStrsLen;

//--------------------------------------------------------------------------------------------------

void Init(Mem permMem) {
	floatingStrs = Mem::AllocT<FloatingStr>(permMem, MaxFloatingStrs);
}

//--------------------------------------------------------------------------------------------------

void CreateFloatingStr(FloatingStrDef def) {
	Assert(floatingStrsLen < MaxFloatingStrs);
	FloatingStr* const fs = &floatingStrs[floatingStrsLen++];
	fs->font   = def.font;
	fs->str    = StrDb::Intern(def.str);
	fs->sec    = 0.f;
	fs->durSec = def.durSec;
	fs->t      = 0.f;
	fs->x      = def.x;
	fs->yStart = def.yStart;
	fs->yEnd   = def.yEnd;
}

//--------------------------------------------------------------------------------------------------

void Frame(F32 sec) {
	for (U32 i = 0; i < floatingStrsLen;) {
		FloatingStr* const fs = &floatingStrs[i];
		fs->sec += sec;
		if (fs->sec >= fs->durSec) {
			floatingStrs[i] = floatingStrs[floatingStrsLen--];
			continue;	// no i++
		}
		fs->t = fs->sec / fs->durSec;
		i++;
	}
}

//--------------------------------------------------------------------------------------------------

void Draw() {
	for (U32 i = 0; i < floatingStrsLen; i++) {
		FloatingStr* const fs = &floatingStrs[i];
		F32 const y = fs->yStart - fs->t * (fs->yStart - fs->yEnd);
		Draw::DrawStr({
			.font = fs->font,
			.str = fs->str,
			.pos = Vec2(fs->x, y),
			.z   = Z_Effect,
			.origin = Draw::Origin::Center,
			.scale = Vec2(1.f, 1.f),
			.color = Vec4(1.f, 0.f, 0.f, 1.f - (fs->t * fs->t)),
			.outlineWidth = 0.f,
			.outlineColor = Vec4(0.f, 0.f, 0.f, 0.f),
		});
		//Logf("text %s at (%.1f, %.1f) (%.2f=%.2f/%.2f)", fs->str, fs->x, y, fs->t, fs->sec, fs->durSec);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Effect