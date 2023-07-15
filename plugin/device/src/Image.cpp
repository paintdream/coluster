#include "Image.h"
#include "Device.h"
#include "CmdBuffer.h"
#include "../ref/vulkansdk/vk_mem_alloc.h"

namespace coluster {
	static const std::unordered_map<std::string_view, VkImageType> imageTypeConstants = {
	    DEFINE_MAP_ENTRY(IMAGE_TYPE_1D),
		DEFINE_MAP_ENTRY(IMAGE_TYPE_2D),
		DEFINE_MAP_ENTRY(IMAGE_TYPE_3D),	
	};

	Image::Image(Device& dev) noexcept : DeviceObject(dev) {}

	bool Image::Initialize(VkImageType type, VkFormat format, uint32_t w, uint32_t h, uint32_t d) {
		auto guard = write_fence();
		if (image != VK_NULL_HANDLE) {
			fprintf(stderr, "[WARNING] Image::Initialize() -> Initializing twice takes no effects!\n");
			return false;
		}

		imageType = type;
		imageFormat = format;
		width = w;
		height = h;
		depth = d;

		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.imageType = type;
		info.format = format;
		info.extent.width = width;
		info.extent.height = height;
		info.extent.depth = depth;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.flags = 0;

		currentLayout = info.initialLayout;

		VmaAllocationCreateInfo vmaAllocInfo = {};
		vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

		device.Verify("create image", vmaCreateImage(device.GetVmaAllocator(), &info, &vmaAllocInfo, &image, &vmaAllocation, nullptr));

		if (image != VK_NULL_HANDLE) {
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.viewType = type == VK_IMAGE_TYPE_1D ? VK_IMAGE_VIEW_TYPE_1D : type == VK_IMAGE_TYPE_2D ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D;
			info.image = image;
			info.format = format;
			info.components.r = VK_COMPONENT_SWIZZLE_R;
			info.components.g = VK_COMPONENT_SWIZZLE_G;
			info.components.b = VK_COMPONENT_SWIZZLE_B;
			info.components.a = VK_COMPONENT_SWIZZLE_A;
			info.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			device.Verify("create image view", vkCreateImageView(device.GetDevice(), &info, device.GetAllocator(), &imageView));
		}

		if (image != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE) {
			return true;
		} else {
			fprintf(stderr, "[ERROR] Image::Initialize() -> Cannot create image or image view.\n");
			return false;
		}
	}

	void Image::Uninitialize() {
		auto guard = write_fence();

		if (imageView != VK_NULL_HANDLE) {
			vkDestroyImageView(device.GetDevice(), imageView, device.GetAllocator());
			imageView = VK_NULL_HANDLE;
		}

		if (image != VK_NULL_HANDLE) {
			vmaDestroyImage(device.GetVmaAllocator(), image, vmaAllocation);
			image = VK_NULL_HANDLE;
		}
	}

	Coroutine<bool> Image::Upload(LuaState lua, Required<CmdBuffer*> cmdBuffer, std::string_view data) {
		if (auto guard = write_fence()) {
			if (image == VK_NULL_HANDLE) {
				fprintf(stderr, "[ERROR] Image::Upload() -> Uninitialized Image!\n");
				co_return false;
			}

			size_t size = data.size();

			VmaAllocationInfo allocInfo = {};
			vmaGetAllocationInfo(device.GetVmaAllocator(), vmaAllocation, &allocInfo);

			if (size > allocInfo.size) {
				fprintf(stderr, "[ERROR] Image::Upload() -> Invalid size!\n");
				co_return false;
			}

			// require both host memory and device memory
			memoryQuotaResource = co_await device.GetAsyncWorker().GetMemoryQuotaQueue().guard({ size, size });

			VmaAllocationCreateInfo vmaAllocInfo = {};
			vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = allocInfo.size;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer uploadBuffer;
			VmaAllocation uploadBufferAllocation;
			device.Verify("create buffer", vmaCreateBuffer(device.GetVmaAllocator(), &bufferInfo, &vmaAllocInfo, &uploadBuffer, &uploadBufferAllocation, nullptr));

			void* map = nullptr;
			device.Verify("map memory", vmaMapMemory(device.GetVmaAllocator(), uploadBufferAllocation, &map));
			memcpy(map, data.data(), size);
			vmaFlushAllocation(device.GetVmaAllocator(), uploadBufferAllocation, 0, size);
			vmaUnmapMemory(device.GetVmaAllocator(), uploadBufferAllocation);

			Warp* currentWarp = co_await Warp::Switch(&cmdBuffer.get()->GetWarp());
			// Copy buffer to image
			VkImageMemoryBarrier copyBarrier = {};
			copyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			copyBarrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT;
			copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			copyBarrier.oldLayout = currentLayout;
			copyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copyBarrier.image = image;
			copyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyBarrier.subresourceRange.baseMipLevel = 0;
			copyBarrier.subresourceRange.levelCount = 1;
			copyBarrier.subresourceRange.baseArrayLayer = 0;
			copyBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyBarrier);

			VkBufferImageCopy region = {};
			region.bufferOffset = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = depth;

			vkCmdCopyBufferToImage(cmdBuffer.get()->GetCommandBuffer(), uploadBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			VkImageMemoryBarrier useBarrier = {};
			useBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			useBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.image = image;
			useBarrier.subresourceRange = copyBarrier.subresourceRange;

			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &useBarrier);
			currentLayout = useBarrier.newLayout;

			co_await cmdBuffer.get()->Submit(lua);
			co_await Warp::Switch(currentWarp);

			vmaDestroyBuffer(device.GetVmaAllocator(), uploadBuffer, uploadBufferAllocation);

			// release cpu memory quota after uploading
			memoryQuotaResource.release({ size, 0 });
		}

		co_return true;
	}

	Coroutine<std::string> Image::Download(LuaState lua, Required<CmdBuffer*> cmdBuffer) {
		std::string data;
		if (auto guard = write_fence()) {
			if (image == VK_NULL_HANDLE) {
				fprintf(stderr, "[ERROR] Image::Download() -> Uninitialized Image!\n");
				co_return "";
			}

			VmaAllocationInfo allocInfo = {};
			vmaGetAllocationInfo(device.GetVmaAllocator(), vmaAllocation, &allocInfo);
			size_t size = iris::iris_verify_cast<size_t>(allocInfo.size);
			VmaAllocationCreateInfo vmaAllocInfo = {};
			vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;

			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer downloadBuffer;
			VmaAllocation downloadBufferAllocation;
			device.Verify("create buffer", vmaCreateBuffer(device.GetVmaAllocator(), &bufferInfo, &vmaAllocInfo, &downloadBuffer, &downloadBufferAllocation, nullptr));

			Warp* currentWarp = co_await Warp::Switch(&cmdBuffer.get()->GetWarp());
			// require both host memory and device memory
			auto downloadQuota = co_await device.GetAsyncWorker().GetMemoryQuotaQueue().guard({ 0, size });

			// Copy image to buffer
			VkImageMemoryBarrier layoutBarrier = {};
			layoutBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			layoutBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			layoutBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			layoutBarrier.oldLayout = currentLayout;
			layoutBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			layoutBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			layoutBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			layoutBarrier.image = image;
			layoutBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			layoutBarrier.subresourceRange.baseMipLevel = 0;
			layoutBarrier.subresourceRange.levelCount = 1;
			layoutBarrier.subresourceRange.baseArrayLayer = 0;
			layoutBarrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &layoutBarrier);

			VkBufferImageCopy region = {};
			region.bufferOffset = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = depth;

			vkCmdCopyImageToBuffer(cmdBuffer.get()->GetCommandBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, downloadBuffer, 1, &region);

			VkImageMemoryBarrier useBarrier = {};
			useBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			useBarrier.newLayout = currentLayout;
			useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.image = image;
			useBarrier.subresourceRange = layoutBarrier.subresourceRange;

			vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &useBarrier);
			co_await cmdBuffer.get()->Submit(lua);

			data.resize(size);

			void* map = nullptr;
			device.Verify("map memory", vmaMapMemory(device.GetVmaAllocator(), downloadBufferAllocation, &map));
			memcpy(data.data(), map, size);
			vmaUnmapMemory(device.GetVmaAllocator(), downloadBufferAllocation);
			vmaDestroyBuffer(device.GetVmaAllocator(), downloadBuffer, downloadBufferAllocation);

			co_await Warp::Switch(currentWarp);
		}

		co_return std::move(data);
	}

	Image::~Image() noexcept {
		Uninitialize();
	}

	void Image::lua_registar(LuaState lua) {
		lua.define<&Image::Initialize>("Initialize");
		lua.define<&Image::Uninitialize>("Uninitialize");
		lua.define<&Image::Upload>("Upload");
		lua.define<&Image::Download>("Download");
		lua.define("ImageType", imageTypeConstants);
	}
}