// Buffer.h
// PaintDream (paintdream@paintdream.com)
// 2023-1-2
//

#pragma once

#include "Device.h"

namespace coluster {
	class Device;
	class CmdBuffer;

	class Buffer : public DeviceObject {
	public:
		Buffer(Device& device) noexcept;
		~Buffer() noexcept;

		Buffer(const Buffer& buffer) = delete;
		Buffer& operator = (const Buffer& rhs) = delete;

		Buffer(Buffer&& rhs) noexcept : DeviceObject(rhs.device), buffer(rhs.buffer), bufferSize(rhs.bufferSize), vmaAllocation(rhs.vmaAllocation) { rhs.buffer = VK_NULL_HANDLE; }
		Buffer& operator = (Buffer&& rhs) noexcept {
			assert(&device == &rhs.GetDevice());
			buffer = rhs.buffer;
			bufferSize = rhs.bufferSize;
			vmaAllocation = rhs.vmaAllocation;
			rhs.buffer = VK_NULL_HANDLE;

			return *this;
		}

		static void lua_registar(LuaState lua);
		bool Initialize(size_t size, bool asUniformBuffer, bool cpuVisible);
		void Uninitialize();

		Coroutine<bool> Upload(LuaState lua, Required<CmdBuffer*> cmdBuffer, size_t offset, std::string_view data);
		Coroutine<std::string> Download(LuaState lua, Required<CmdBuffer*> cmdBuffer, size_t offset, size_t size);

		VkBuffer GetBuffer() const noexcept { return buffer; }
		size_t GetBufferSize() const noexcept { return bufferSize; }
		VkBufferUsageFlags GetUsage() const noexcept { return bufferUsage; }

	protected:
		VkBuffer buffer = VK_NULL_HANDLE;
		size_t bufferSize = 0;
		VkBufferUsageFlags bufferUsage = 0;
		VmaAllocation vmaAllocation = nullptr;
		AsyncWorker::MemoryQuotaQueue::resource_t memoryQuotaResource;
	};
}

