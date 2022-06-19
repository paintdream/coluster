// worker.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-14
//

#pragma once

#include "../common.h"
#include <string>

namespace coluster {
	class framework_t {	
	public:
		class listener_t {
		public:
			virtual void frame_tick(scalar dtime) = 0;
		};

		virtual void register_listener(listener_t* listener) = 0;
		virtual void unregister_listener(listener_t* listener) = 0;
		virtual void register_procedure(const char* name, std::function<std::string(std::string_view)>&& func) = 0;
		virtual void unregister_procedure(const char* name) = 0;
	};
}
