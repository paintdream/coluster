#include "DataPipe.h"

namespace coluster {
	DataPipe::~DataPipe() noexcept {}

	void DataPipe::lua_registar(LuaState lua) {
		lua.set_current<&DataPipe::Push>("Push");
		lua.set_current<&DataPipe::Pop>("Pop");
		lua.set_current<&DataPipe::Empty>("Empty");
	}

	bool DataPipe::Empty() const noexcept {
		auto guard = out_fence();
		return dataQueueList.probe(sizeof(size_t) + 1);
	}

	void DataPipe::Push(std::string_view data) {
		auto guard = in_fence();
		size_t size = data.size();
		if (size != 0) {
			dataQueueList.push(reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size) + sizeof(size_t));
			dataQueueList.push(data.data(), data.data() + data.length());
		}
	}

	std::string DataPipe::Pop(LuaState state) {
		if (Empty()) {
			return "";
		}

		auto guard = out_fence();
		size_t size = 0;
		dataQueueList.pop(reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size) + sizeof(size_t));

		std::string data;
		data.resize(size);
		dataQueueList.pop(data.data(), data.data() + size);

		return data;
	}
}