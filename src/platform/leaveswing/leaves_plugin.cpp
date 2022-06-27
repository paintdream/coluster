#include "leaves_plugin.h"
#include "IPlugin.h"
#include <cassert>
using namespace PaintsNow;

namespace coluster {
	static IPlugin* global_plugin = nullptr;

	static std::string global_response;
	static std::atomic<size_t> global_fence = 0;

	template <leaves_plugin_t::method_t procedure>
	const char* leaves_plugin_t::request_method_handler(const char* request, unsigned long& len, void* context) {
		leaves_plugin_t* instance = reinterpret_cast<leaves_plugin_t*>(context);
		assert(instance != nullptr);
		instance->acquire_write();

		std::string response = (instance->*procedure)(std::string_view(request, len));
		len = grid::verify_cast<unsigned long>(response.size());
		return len == 0 ? nullptr : (instance->plugin_response = std::move(response)).c_str();
	}

	template <leaves_plugin_t::function_t procedure>
	const char* leaves_plugin_t::request_function_handler(const char* request, unsigned long& len, void* context) {
		assert(global_fence.exchange(1u, std::memory_order_acquire) == 0u);

		leaves_plugin_t* instance = reinterpret_cast<leaves_plugin_t*>(context);
		assert(instance != nullptr);
		std::string response = procedure(std::string_view(request, len));
		len = grid::verify_cast<unsigned long>(response.size());
		return len == 0 ? nullptr : (global_response = std::move(response)).c_str();
	}

	void leaves_plugin_t::response_function_handler(const char* request, unsigned long len, void* context) {
		assert(global_fence.exchange(0u, std::memory_order_release) == 1u);
	}

	void leaves_plugin_t::response_method_handler(const char* request, unsigned long len, void* context) {
		leaves_plugin_t* leaves_plugin = reinterpret_cast<leaves_plugin_t*>(context);
		assert(leaves_plugin != nullptr);

		leaves_plugin->release_write();
	}

	template <leaves_plugin_t::function_t procedure>
	void leaves_plugin_t::register_function_handler(void* script, const char* name) {
		global_plugin->RegisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), name, &request_function_handler<procedure>, response_function_handler, nullptr);
	}

	template <leaves_plugin_t::method_t procedure>
	void leaves_plugin_t::register_method_handler(void* script, const char* name) {
		global_plugin->RegisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), name, &request_method_handler<procedure>, response_method_handler, this);
	}

	void leaves_plugin_t::unregister_handler(void* script, const char* name) {
		global_plugin->UnregisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), name);
	}

	struct alignas(64) Task : public IPlugin::Task {
	public:
		Task(std::function<void()>&& f) noexcept : func(std::move(f)) {}

		void Execute() noexcept override final {
			func();
			this->~Task();

			global_plugin->FreeMemory(this, sizeof(*this));
		}

		void Abort() noexcept override final {
			Execute();
		}

		std::function<void()> func;
	};

	leaves_plugin_t::leaves_plugin_t() noexcept {
		script = global_plugin->AllocateScript();
	}

	leaves_plugin_t::~leaves_plugin_t() {
		global_plugin->FreeScript(reinterpret_cast<IPlugin::Script*>(script));
	}

	size_t leaves_plugin_t::get_thread_count() const noexcept {
		assert(global_plugin != nullptr);
		return global_plugin->GetThreadCount();
	}

	size_t leaves_plugin_t::get_current_thread_index() const noexcept {
		assert(global_plugin != nullptr);
		return global_plugin->GetCurrentThreadIndex();
	}

	void leaves_plugin_t::queue(std::function<void()>&& func, size_t priority) {
		assert(global_plugin != nullptr);
		global_plugin->QueueTask(new (global_plugin->AllocateMemory(sizeof(Task))) Task(std::move(func)), static_cast<int>(priority));
	}

	struct frame_ticker_params {
		uint32_t dtime; // dtime in milliseconds
	};

	std::string leaves_plugin_t::handle_frame_ticker(std::string_view request) {
		// parse delta time from request
		if (request.size() >= sizeof(frame_ticker_params)) {
			const frame_ticker_params& params = *reinterpret_cast<const frame_ticker_params*>(request.data());
			scalar dtime = scalar(params.dtime) / scalar(1000);
			if (listener != nullptr) {
				listener->frame_tick(dtime);
			}
		}

		return {};
	}

	void leaves_plugin_t::bind_listener(listener_t* instance) {
		auto guard = write_fence();
		assert(listener != instance);
		listener = instance;

		if (instance != nullptr) {
			register_method_handler<&leaves_plugin_t::handle_frame_ticker>(reinterpret_cast<IPlugin::Script*>(script), "coluster_frame_ticker");
		} else {
			unregister_handler(reinterpret_cast<IPlugin::Script*>(script), "coluster_frame_ticker");
		}
	}

	const char* leaves_plugin_t::handle_procedure_request(const char* request, unsigned long& len, void* context) {
		procedure_context_t& proc = *reinterpret_cast<procedure_context_t*>(context);
		proc.plugin->acquire_write();
		std::string response = proc.func(std::string_view(request, len));
		len = grid::verify_cast<unsigned long>(response.size());
		return len == 0 ? nullptr : (proc.plugin->plugin_response = std::move(response)).c_str();
	}

	void leaves_plugin_t::handle_procedure_response(const char* request, unsigned long len, void* context) {
		procedure_context_t& proc = *reinterpret_cast<procedure_context_t*>(context);
		proc.plugin->release_write();
	}

	void leaves_plugin_t::register_procedure(const char* name, procedure_t&& func) {
		assert(global_plugin != nullptr);

		auto guard = write_fence();
		procedure_context_t& proc = procedures[name];
		proc.func = std::move(func);
		proc.plugin = this;
		global_plugin->RegisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), name, &leaves_plugin_t::handle_procedure_request, &leaves_plugin_t::handle_procedure_response, &proc);
	}

	void leaves_plugin_t::unregister_procedure(const char* name) {
		assert(global_plugin != nullptr);

		auto guard = write_fence();
		global_plugin->UnregisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), name);
		procedures.erase(name);
	}
}

// From http://gcc.gnu.org/wiki/Visibility

#if defined _WIN32 || defined __CYGWIN__
#ifdef __GNUC__
#define DLL_PUBLIC __attribute__ ((dllexport))
#else
#define DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif

using namespace coluster;

static std::string create_leaves_plugin(std::string_view request) {
	std::string str;
	str.resize(sizeof(uint64_t));
	leaves_plugin_t* instance = new leaves_plugin_t();
	memcpy(str.data(), &instance, sizeof(instance));

	return str;
}

static std::string delete_leaves_plugin(std::string_view request) {
	assert(request.size() >= sizeof(size_t));
	leaves_plugin_t* plugin = *reinterpret_cast<leaves_plugin_t* const*>(request.data());
	delete plugin;

	return {};
}

extern "C" DLL_PUBLIC bool LeavesMain(IPlugin* plugin) {
	global_plugin = plugin;
	leaves_plugin_t::register_function_handler<&create_leaves_plugin>(nullptr, "create_leaves_plugin");
	leaves_plugin_t::register_function_handler<&delete_leaves_plugin>(nullptr, "delete_leaves_plugin");

	return true;
}