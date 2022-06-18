// geometry.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-7

#pragma once

#include "../common.h"

namespace coluster {
	struct transform_t {
		vec3 position;
		scalar scale; // uniform scaling
		vec4 rotation; // represent as quaternion
	};
	
	struct motion_t {
		vec3 velocity;
		scalar angular_velocity;
	};

	struct derivative_motion_t {
		vec3 acceleration;
		scalar angular_acceleration;
	};
}
