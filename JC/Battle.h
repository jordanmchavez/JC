#pragma once

#include "JC/Common.h"

namespace JC::App    { struct UpdateData; }
namespace JC::Window { struct State; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

void  Init(Mem permMem, Mem tempMem, Window::State const* windowState);
Res<> LoadMap(Str path);
Res<> Update(App::UpdateData const* appUpdateData);
void  Draw();

void GenerateRandomMap(U32 cols, U32 rows);
void GenerateRandomArmies();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle