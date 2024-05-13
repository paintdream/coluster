#include "DataBuffer.h"

namespace coluster {
	DataBuffer::DataBuffer(AsyncWorker& worker) : asyncWorker(worker) {}
	DataBuffer::~DataBuffer() noexcept {}

	void DataBuffer::lua_registar(LuaState lua) {
		lua.set_current<&DataBuffer::Resize>("Resize");
		lua.set_current<&DataBuffer::Read>("Read");
		lua.set_current<&DataBuffer::Write>("Write");
		lua.set_current<&DataBuffer::Copy>("Copy");
	}

	void DataBuffer::lua_initialize(LuaState lua, int index) noexcept {}

	Coroutine<void> DataBuffer::Resize(size_t length) {
		memoryQuotaResource = co_await asyncWorker.GetMemoryQuotaQueue().guard({ length, 0 });
		buffer.resize(length);
	}

	std::string_view DataBuffer::Read(size_t offset, size_t length) {
		offset = std::min(buffer.size(), offset);
		size_t limit = std::min(buffer.size(), offset + length);
		return std::string_view(buffer.data() + offset, limit - offset);
	}

	void DataBuffer::Write(size_t offset, std::string_view data) {
		offset = std::min(buffer.size(), offset);
		size_t limit = std::min(buffer.size(), offset + data.size());
		size_t length = limit - offset;
		if (length != 0) {
			memcpy(buffer.data() + offset, data.data(), length);
		}
	}

	void DataBuffer::Copy(Required<DataBuffer*>&& target, size_t sourceOffset, size_t targetOffset, size_t length) {
		DataBuffer& rhs = *target.get();
		sourceOffset = std::min(buffer.size(), sourceOffset);
		targetOffset = std::min(rhs.buffer.size(), targetOffset);
		size_t sourceLimit = std::min(buffer.size(), sourceOffset + length);
		size_t targetLimit = std::min(rhs.buffer.size(), targetOffset + length);
		length = std::min(sourceLimit - sourceOffset, targetLimit - targetOffset);

		if (length != 0) {
			memcpy(rhs.buffer.data() + targetOffset, buffer.data() + sourceOffset, length);
		}
	}
}