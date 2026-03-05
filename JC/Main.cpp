#include "JC/App.h"

using namespace JC;

namespace JC::Game { App::App* GetApp(); }

int main(int argc, char const** argv) {
	return App::Run(Game::GetApp(), argc, argv) ? 0 : 1;
}