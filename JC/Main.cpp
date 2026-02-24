#include "JC/App.h"
#include "JC/Unit.h"

using namespace JC;

namespace JC::Battle { App::App* GetApp(); }

int main(int argc, char const** argv) {
	if (argc == 2 && argv[1] == Str("test")) {
		Unit::Run(); return 0;
	}

	return App::Run(Battle::GetApp(), argc, argv) ? 0 : 1;
}