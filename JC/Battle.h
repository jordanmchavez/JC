#pragma once

#include "JC/Common.h"

namespace JC::App    { struct FrameData; }
namespace JC::Window { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

Res<> Init(Mem permMem, Mem tempMem, Window::State const* windowState);
Res<> Load(Str path);
Res<> GenerateMap();
Res<> Frame(App::FrameData const* appFrameData);
void  Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle