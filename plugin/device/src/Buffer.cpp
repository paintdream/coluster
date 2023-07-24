#include "Buffer.h"
#include "Device.h"
#include "CmdBuffer.h"
#include "../ref/vulkansdk/vk_mem_alloc.h"

namespace coluster {
	Buffer::Buffer(Device& dev) noexcept : DeviceObject(dev) {}
	
	bool Buffer::Initialize(size_t size, bool asUniformBuffer, bool cpuVisible) {
		auto guard = write_fence();
		if (buffer != VK_NULL_HANDLE) {
			fprintf(stderr, "[WARNING] Buffer::Initialize() -> Initializing twice takes no effects!\n");
			return false;
		}

		bufferSize = size;

		VkBufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size = size;
		createInfo.usage = (asUniformBuffer ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		bufferUsage = createInfo.usage;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = cpuVisible ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_AUTO;
		device.Verify("create buffer", vmaCreateBuffer(device.GetVmaAllocator(), &createInfo, &allocInfo, &buffer, &vmaAllocation, nullptr));

		return buffer != VK_NULL_HANDLE;
	}

	void Buffer::Uninitialize() {
		auto guard = write_fence();

		if (buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(device.GetVmaAllocator(), buffer, vmaAllocation);
			buffer = VK_NULL_HANDLE;
		}
	}

	Buffer::~Buffer() noexcept {
		Uninitialize();
	}

	Coroutine<bool> Buffer::Upload(LuaState lua, Required<CmdBuffer*> cmdBuffer, size_t offset, std::string_view data) {
		if (auto guard = write_fence()) {
			if (buffer == VK_NULL_HANDLE) {
				fprintf(stderr, "[ERROR] Buffer::Upload() -> Uninitialized buffer!\n");
				co_return false;
			}

			if (data.empty() || offset + data.size() > bufferSize) {
				fprintf(stderr, "[ERROR] Buffer::Upload() -> Input data size error!\n");
				co_return false;
			}

			size_t size = data.size();
			// require both host memory and device memory
			memoryQuotaResource = co_await device.GetAsyncWorker().GetMemoryQuotaQueue().guard({ 0, size * 2 });

			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo vmaAllocInfo = {};
			vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
			VkBuffer uploadBuffer;
			VmaAllocation uploadBufferAllocation;
			device.Verify("create buffer", vmaCreateBuffer(device.GetVmaAllocator(), &bufferInfo, &vmaAllocInfo, &uploadBuffer, &uploadBufferAllocation, nullptr));

			void* map = nullptr;
			device.Verify("map memory", vmaMapMemory(device.GetVmaAllocator(), uploadBufferAllocation, &map));
			memcpy(map, data.data(), size);
			vmaFlushAllocation(device.GetVmaAllocator(), uploadBufferAllocation, 0, size);
			vmaUnmapMemory(device.GetVmaAllocator(), uploadBufferAllocation);

			Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &cmdBuffer.get()->GetWarp());

			VkBufferMemoryBarrier copyBarrier = {};
			copyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			copyBarrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT;
			copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.buffer = uploadBuffer;
			copyBarrier.offset = 0;
			copyBarrier.size = size;

			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &copyBarrier, 0, nullptr);

			VkBufferCopy bufferCopy = {};
			bufferCopy.srcOffset = 0;
			bufferCopy.dstOffset = offset;
			bufferCopy.size = size;

			vkCmdCopyBuffer(cmdBuffer.get()->GetCommandBuffer(), uploadBuffer, buffer, 1, &bufferCopy);

			VkBufferMemoryBarrier useBarrier = {};
			useBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.buffer = buffer;
			useBarrier.offset = offset;
			useBarrier.size = size;

			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &useBarrier, 0, nullptr);
			co_await cmdBuffer.get()->Submit(lua);

			vmaDestroyBuffer(GetDevice().GetVmaAllocator(), uploadBuffer, uploadBufferAllocation);
			co_await Warp::Switch(std::source_location::current(), currentWarp);

			// release staging part
			memoryQuotaResource.release({0, size});
		}

		co_return true;
	}

	Coroutine<std::string> Buffer::Download(LuaState lua, Required<CmdBuffer*> cmdBuffer, size_t offset, size_t size) {
		std::string data;
		if (auto guard = write_fence()) {
			if (buffer == VK_NULL_HANDLE) {
				fprintf(stderr, "[ERROR] Buffer::Download() -> Uninitialized buffer!\n");
				co_return "";
			}

			if (offset + size > bufferSize) {
				fprintf(stderr, "[ERROR] Buffer::Download() -> Output data size error!\n");
				co_return "";
			}

			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo vmaAllocInfo = {};
			vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
			VkBuffer downloadBuffer;
			VmaAllocation downloadBufferAllocation;
			device.Verify("create buffer", vmaCreateBuffer(device.GetVmaAllocator(), &bufferInfo, &vmaAllocInfo, &downloadBuffer, &downloadBufferAllocation, nullptr));

			Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &cmdBuffer.get()->GetWarp());
			auto downloadQuota = co_await device.GetAsyncWorker().GetMemoryQuotaQueue().guard({ 0, size }); 

			VkBufferMemoryBarrier useBarrier = {};
			useBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			useBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.buffer = buffer;
			useBarrier.offset = offset;
			useBarrier.size = size;

			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &useBarrier, 0, nullptr);

			VkBufferCopy bufferCopy = {};
			bufferCopy.srcOffset = offset;
			bufferCopy.dstOffset = 0;
			bufferCopy.size = size;

			vkCmdCopyBuffer(cmdBuffer.get()->GetCommandBuffer(), buffer, downloadBuffer, 1, &bufferCopy);

			VkBufferMemoryBarrier copyBarrier = {};
			copyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			copyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.buffer = downloadBuffer;
			copyBarrier.offset = 0;
			copyBarrier.size = size;

			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &copyBarrier, 0, nullptr);

			co_await cmdBuffer.get()->Submit(lua);
			co_await Warp::Switch(std::source_location::current(), currentWarp);

			data.resize(size);

			void* map = nullptr;
			device.Verify("map memory", vmaMapMemory(device.GetVmaAllocator(), downloadBufferAllocation, &map));
			memcpy(data.data(), map, size);
			vmaFlushAllocation(device.GetVmaAllocator(), downloadBufferAllocation, 0, size);
			vmaUnmapMemory(device.GetVmaAllocator(), downloadBufferAllocation);

			vmaDestroyBuffer(device.GetVmaAllocator(), downloadBuffer, downloadBufferAllocation);
		}

		co_return std::move(data);
	}

	void Buffer::lua_registar(LuaState lua) {
		lua.define<&Buffer::Initialize>("Initialize");
		lua.define<&Buffer::Uninitialize>("Uninitialize");
		lua.define<&Buffer::Upload>("Upload");
		lua.define<&Buffer::Download>("Download");
	}
}

