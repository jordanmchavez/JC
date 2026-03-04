#pragma once

#include "JC/Common.h"

namespace JC::Draw { DefHandle(Font); }

namespace JC::Effect {

//--------------------------------------------------------------------------------------------------

struct FloatingStrDef {
	Draw::Font font;
	Str        str;
	F32        durSec;
	F32        x;
	F32        yStart;
	F32        yEnd;
};

void Init(Mem permMem);
void CreateFloatingStr(FloatingStrDef def);
void Frame(F32 sec);
void Draw(F32 z);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Effect