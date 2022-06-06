#include "sprite.h"

namespace coluster {
	coroutine_t sprite_t::tick() {
		co_return;
	}
}

using namespace coluster;

int main(int argc, char* argv[]) {
	[[maybe_unused]] sprite_t sprite;
	return 0;
}
