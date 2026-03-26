#pragma once

#include "JC/Common.h"

namespace JC::App    { struct UpdateData; }
namespace JC::Window { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

void  Init(Mem permMem, Mem tempMem, Window::State const* windowState);
Res<> Load(Str path);
Res<> Update(App::UpdateData const* appUpdateData);
void  Draw();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle