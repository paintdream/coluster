#include "DataPipe.h"

namespace coluster {
	DataPipe::~DataPipe() noexcept {}
	void DataPipe::lua_initialize(LuaState lua, int index) noexcept {
		// by default, inputWarp == outputWarp == scriptWarp
		Warp* currentWarp = Warp::get_current_warp();
		inputWarp = currentWarp;
		outputWarp = outputWarp;
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

	void DataPipe::CheckedPush(RequiredDataPipe<true>&& self, std::string_view data) {
		self->Push(data);
	}

	std::string DataPipe::CheckedPop(RequiredDataPipe<false>&& self) {
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

	void DataPipe::Push(std::string_view data) {
		assert(Warp::get_current_warp() == inputWarp);
		auto guard = in_fence();
		size_t size = data.size();
		if (size != 0) {
			dataQueueList.push(reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size) + sizeof(size_t));
			dataQueueList.push(data.data(), data.data() + data.size());
		}
	}

	std::string DataPipe::Pop() {
		assert(Warp::get_current_warp() == outputWarp);
		auto guard = out_fence();
		size_t size = 0;
		dataQueueList.pop(reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size) + sizeof(size_t));

		std::string data;
		data.resize(size);
		dataQueueList.pop(data.data(), data.data() + size);

		return data;
	}
}