// storage.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-7

#pragma once

#include "common.h"
#include "properties/quantity.h"
#include "properties/geometry.h"

namespace coluster {
	class storage_t {
	public:
		properties_t<transform_t> states;
		properties_t<motion_t, derivative_motion_t> motions;
		properties_t<temperature_t, pressure_t, mass_t> quantities;
	};
}
