#include "JC/App.h"
#include "JC/Game_Shmup.h"
#include "JC/UnitTest.h"

using namespace JC;

//--------------------------------------------------------------------------------------------------

int main(U32 argc, const char* argv[])
{
	RunApp(GetGame(), argc, argv);
	return 0;
}