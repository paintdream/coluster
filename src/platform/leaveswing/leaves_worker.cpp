#include "leaves_worker.h"
#include "IPlugin.h"
#include <cassert>
using namespace PaintsNow;

namespace coluster {
	static IPlugin* GPlugin = nullptr;
	struct Task : public IPlugin::Task {
	public:
		Task(std::function<void()>&& f) noexcept : func(std::move(f)) {}

		void Execute() noexcept override final {
			func();
			delete this;
		}

		void Abort() noexcept override final {
			Execute();
		}

		std::function<void()> func;
	};

	size_t leaves_worker_t::get_current_thread_index() const noexcept {
		assert(GPlugin != nullptr);
		return GPlugin->GetCurrentThreadIndex();
	}

	void leaves_worker_t::queue(std::function<void()>&& func, size_t priority) {
		assert(GPlugin != nullptr);
		GPlugin->QueueTask(new Task(std::move(func)), static_cast<int>(priority));
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
	coluster::GPlugin = plugin;
	return true;
}