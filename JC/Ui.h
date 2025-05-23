#pragma once

#include "JC/Core.h"

namespace JC::Event { struct Event; };

namespace JC::Ui {

//--------------------------------------------------------------------------------------------------

enum struct SizeType {
	Fixed,
	ParentPct,
	ChildSum,
};

struct Elem {
	SizeType sizeType;
};

void  Init();
void  Shutdown();
void  Clear();
Res<> Events(Span<Event::Event> events);
void  MakeBox(SizeType sizeTypeX, SizeType sizeTypeY, Vec2 size, F32 parentPct, Vec4 color);
void  Draw();

//--------------------------------------------------------------------------------------------------

}	// JC::Ui