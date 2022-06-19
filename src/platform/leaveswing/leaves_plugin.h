// worker.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-14
//

#pragma once

#include "../worker.h"
#include "../framework.h"
#include <map>

namespace coluster {
	class leaves_plugin_t : public worker_t, public framework_t {
	public:
		leaves_plugin_t() noexcept;
		void register_listener(listener_t* listener) override;
		void unregister_listener(listener_t* listener) override;
		void register_procedure(const char* name, std::function<std::string(std::string_view)>&& func);
		void unregister_procedure(const char* name);
		size_t get_current_thread_index() const noexcept override;
		size_t get_thread_count() const noexcept override;
		void queue(std::function<void()>&& func, size_t priority = 0) override;

		static const char* handle_frame_ticker(const char* request, unsigned long& len, void* context);
		static const char* handle_procedure(const char* request, unsigned long& len, void* context);

	protected:
		std::atomic<size_t> plugin_fence;

		struct procedure_t {
			leaves_plugin_t* plugin;
			std::string return_value;
			std::function<std::string(std::string_view)> func;
		};

		std::map<std::string, procedure_t> procedures;
	};
}