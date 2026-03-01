#include "JC/App.h"

using namespace JC;

namespace JC::Battle { App::App* GetApp(); }

int main(int argc, char const** argv) {
	return App::Run(Battle::GetApp(), argc, argv) ? 0 : 1;
}