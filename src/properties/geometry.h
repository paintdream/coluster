// geometry.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-7

#pragma once

#include "../common.h"

namespace coluster {
	struct transform_t {
		vec3 position;
		float scale; // uniform scaling
		vec4 rotation; // represent as quaternion
	};
	
	struct motion_t {
		vec3 velocity;
		float angular_velocity;
	};

	struct derivative_motion_t {
		vec3 acceleration;
		float angular_acceleration;
	};
}
