#include "DataPipe.h"

namespace coluster {
	DataPipe::~DataPipe() noexcept {}

	void DataPipe::lua_registar(LuaState lua) {
		lua.set_current<&DataPipe::Push>("Push");
		lua.set_current<&DataPipe::Pop>("Pop");
		lua.set_current<&DataPipe::Empty>("Empty");
	}

	bool DataPipe::Empty() const noexcept {
		return lengthStream.empty();
	}

	void DataPipe::Push(std::string_view data) {
		dataStream.push(data.data(), data.data() + data.length());
		lengthStream.push(data.length());
	}

	std::string DataPipe::Pop(LuaState state) {
		if (lengthStream.empty()) {
			return "";
		}

		size_t length = lengthStream.top();
		lengthStream.pop();

		std::string data;
		data.resize(length);
		dataStream.pop(data.data(), data.data() + length);
		dataStream.pop(length);

		return data;
	}
}