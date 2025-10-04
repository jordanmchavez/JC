#include "JC/Common.h"
#include "JC/Unit.h"

int main(int argc, const char** argv) {
	if (argc == 2 && argv[1] == Str("test")) {
		Unit_Run();
	}

	return 0;
}