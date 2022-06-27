// worker.h
// PaintDream (paintdream@paintdream.com)
// 2022-6-14
//

#pragma once

#include "../ref/grid_dispatcher/grid_common.h"
#include "../worker.h"
#include "../framework.h"
#include <map>

namespace coluster {
	class leaves_plugin_t : public worker_t, public framework_t, protected grid::enable_read_write_fence_t<> {
	public:
		leaves_plugin_t() noexcept;
		virtual ~leaves_plugin_t();

		using procedure_t = std::function<std::string(std::string_view)>;
		using method_t = std::string(leaves_plugin_t::*)(std::string_view);
		using function_t = std::string(*)(std::string_view);

		void bind_listener(listener_t* listener) override;
		void register_procedure(const char* name, procedure_t&& func);
		void unregister_procedure(const char* name);
		size_t get_current_thread_index() const noexcept override;
		size_t get_thread_count() const noexcept override;
		void queue(std::function<void()>&& func, size_t priority = 0) override;

		template <function_t procedure>
		static void register_function_handler(void* script, const char* name);
		static void unregister_handler(void* script, const char* name);

	protected:
		template <method_t procedure>
		void register_method_handler(void* script, const char* name);
		template <method_t procedure>
		static const char* request_method_handler(const char* request, unsigned long& len, void* context);
		template <function_t procedure>
		static const char* request_function_handler(const char* request, unsigned long& len, void* context);
		static void response_function_handler(const char* request, unsigned long len, void* context);
		static void response_method_handler(const char* request, unsigned long len, void* context);

	protected:
		std::string handle_frame_ticker(std::string_view request);
		static const char* handle_procedure_request(const char* request, unsigned long& len, void* context);
		static void handle_procedure_response(const char* request, unsigned long len, void* context);

		struct procedure_context_t {
			leaves_plugin_t* plugin;
			std::function<std::string(std::string_view)> func;
		};

	protected:
		listener_t* listener = nullptr;
		void* script = nullptr;
		std::string plugin_response;
		std::map<std::string, procedure_context_t> procedures;
	};
}