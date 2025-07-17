#include "JC/App.h"

namespace JC::App3D { extern App::Fns appFns; };

int main(int argc, const char* argv[])
{
	JC::App::Run(&JC::App3D::appFns, argc, argv);
	return 0;
}