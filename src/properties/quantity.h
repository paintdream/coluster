// quantity.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-7

#pragma once

#include "../common.h"

namespace coluster {
	struct temperature_t {
		vec3 gradient_temperature; // dT/dx, dT/dy, dT/dz
		scalar value;
	};

	struct pressure_t {
		vec3 gradient_pressure; // dp/dx, dp/dy, dp/dz
		scalar pressure;
	};

	struct mass_t {
		scalar value;
	};
}
