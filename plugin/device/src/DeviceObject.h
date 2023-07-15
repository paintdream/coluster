// DeviceObject.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-31
//

#pragma once

#include "Pipeline.h"

namespace coluster {
	class Device;
	class DeviceObject : protected EnableReadWriteFence {
	public:
		DeviceObject(Device& device) noexcept;
		Device& GetDevice() noexcept { return device; }

	protected:
		Device& device;
	};
}
