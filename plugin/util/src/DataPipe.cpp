#include "DataPipe.h"

namespace coluster {
	DataPipe::DataPipe(AsyncWorker& asyncWorker) : asyncPipe(asyncWorker) {}
	DataPipe::~DataPipe() noexcept {}

	void DataPipe::lua_initialize(LuaState lua, int index) noexcept {
		// by default, inputWarp == outputWarp == scriptWarp
		Warp* currentWarp = Warp::get_current_warp();
		inputWarp = currentWarp;
		outputWarp = currentWarp;
	}

	bool DataPipe::BindInputWarp(Warp* warp) noexcept {
		if (Warp::get_current_warp() == inputWarp) {
			inputWarp = warp;
			return true;
		} else {
			return false;
		}
	}

	bool DataPipe::BindOutputWarp(Warp* warp) noexcept {
		if (Warp::get_current_warp() == outputWarp) {
			outputWarp = warp;
			return true;
		} else {
			return false;
		}
	}

	void DataPipe::lua_registar(LuaState lua) {
		lua.set_current<&DataPipe::CheckedPush>("Push");
		lua.set_current<&DataPipe::CheckedPop>("Pop");
		lua.set_current<&DataPipe::CheckedEmpty>("Empty");
	}

	Coroutine<void> DataPipe::CheckedPush(RequiredDataPipe<true>&& self, std::string_view data) {
		return self->Push(data);
	}

	Coroutine<std::string> DataPipe::CheckedPop(RequiredDataPipe<false>&& self) {
		return self->Pop();
	}

	bool DataPipe::CheckedEmpty(RequiredDataPipe<false>&& self) {
		return self->Empty();
	}

	bool DataPipe::Empty() const noexcept {
		assert(Warp::get_current_warp() == outputWarp);
		auto guard = out_fence();
		return dataQueueList.probe(sizeof(size_t) + 1);
	}

	Coroutine<void> DataPipe::Push(std::string_view data) {
		assert(Warp::get_current_warp() == inputWarp);
		memoryQuotaResource.merge(co_await asyncPipe.get_async_worker().GetMemoryQuotaQueue().guard({ data.size(), 0 }));
		auto guard = in_fence();
		dataQueueList.push(data.data(), data.data() + data.size());
		asyncPipe.emplace(data.size());
	}

	Coroutine<std::string> DataPipe::Pop() {
		assert(Warp::get_current_warp() == outputWarp);
		size_t size = co_await asyncPipe;
		auto guard = out_fence();
		std::string data;
		data.resize(size);
		dataQueueList.pop(data.data(), data.data() + size);
		memoryQuotaResource.release({ data.size(), 0 });

		co_return std::move(data);
	}
}