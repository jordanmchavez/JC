#include "JC/Unit.h"

using namespace JC;

int main(int argc, const char** argv) {
	if (argc == 2 && argv[1] == Str("test")) {
		Unit::Run();
	}

	return 0;
}