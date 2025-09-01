#include "JC/App.h"

namespace JC::AppShmup { extern App::Fns appFns; };

int main(int argc, const char* argv[])
{
	JC::App::Run(&JC::AppShmup::appFns, argc, argv);
	return 0;
}