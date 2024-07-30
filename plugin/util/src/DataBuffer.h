// DataBuffer.h
// PaintDream (paintdream@paintdream.com)
// 2024-1-5
//

#pragma once

#include "../../../src/Coluster.h"

namespace coluster {
	class DataBuffer : public Object, protected EnableReadWriteFence {
	public:
		DataBuffer(AsyncWorker& asyncWorker);
		~DataBuffer() noexcept override;
		static void lua_registar(LuaState lua);
		void lua_initialize(LuaState lua, int index) noexcept;

		Coroutine<void> Resize(size_t length);
		std::string_view Read(size_t offset, size_t length);
		void Write(size_t offset, std::string_view data);
		void Copy(Required<DataBuffer*>&& target, size_t sourceOffset, size_t targetOffset, size_t length);

	protected:
		AsyncWorker& asyncWorker;
		AsyncWorker::MemoryQuotaQueue::resource_t memoryQuotaResource;
		std::vector<char> buffer;
	};
}
