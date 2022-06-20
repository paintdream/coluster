#include "leaves_plugin.h"
#include "IPlugin.h"
#include <cassert>
using namespace PaintsNow;

namespace coluster {
	static IPlugin* global_plugin = nullptr;

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

	leaves_plugin_t::leaves_plugin_t() noexcept : listener(nullptr) {
		plugin_fence.store(0, std::memory_order_relaxed);
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

	const char* leaves_plugin_t::handle_frame_ticker(const char* request, unsigned long& len, void* context) {
		// parse delta time from request
		if (len >= sizeof(frame_ticker_params)) {
			const frame_ticker_params& params = *reinterpret_cast<const frame_ticker_params*>(request);
			scalar dtime = scalar(params.dtime) / scalar(1000);
			leaves_plugin_t* plugin = reinterpret_cast<leaves_plugin_t*>(context);

			if (plugin->listener != nullptr) {
				plugin->listener->frame_tick(dtime);
			}
		}

		return nullptr;
	}

	void leaves_plugin_t::bind_listener(listener_t* instance) {
		auto guard = grid::write_fence(plugin_fence);
		assert(listener != instance);
		listener = instance;

		if (instance != nullptr) {
			global_plugin->RegisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), "coluster_frame_ticker", &coluster::leaves_plugin_t::handle_frame_ticker, nullptr, this);
		} else {
			global_plugin->UnregisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), "coluster_frame_ticker");
		}
	}

	const char* leaves_plugin_t::handle_procedure(const char* request, unsigned long& len, void* context) {
		procedure_t& proc = *reinterpret_cast<procedure_t*>(context);
		auto guard = grid::read_fence(proc.plugin->plugin_fence);

		proc.return_value = proc.func(std::string_view(request, len));
		len = grid::verify_cast<unsigned long>(proc.return_value.size());

		return proc.return_value.c_str();
	}

	void leaves_plugin_t::register_procedure(const char* name, std::function<std::string(std::string_view)>&& func) {
		assert(global_plugin != nullptr);

		auto guard = grid::write_fence(plugin_fence);
		procedure_t& proc = procedures[name];
		proc.func = std::move(func);
		proc.plugin = this;
		global_plugin->RegisterScriptHandler(reinterpret_cast<IPlugin::Script*>(script), name, &coluster::leaves_plugin_t::handle_procedure, nullptr, &proc);
	}

	void leaves_plugin_t::unregister_procedure(const char* name) {
		assert(global_plugin != nullptr);

		auto guard = grid::write_fence(plugin_fence);
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

extern "C" DLL_PUBLIC bool LeavesMain(IPlugin * plugin) {
	coluster::global_plugin = plugin;
	return true;
}