// sprite.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-5

#pragma once

#include "common.h"

namespace coluster {
	class sprite_t {
	public:
		coroutine_t tick(scalar dtime);
		id_t id;
	};
}