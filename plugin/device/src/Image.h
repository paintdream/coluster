// Image.h
// PaintDream (paintdream@paintdream.com)
// 2023-1-2
//

#pragma once

#include "DeviceObject.h"

namespace coluster {
	class Device;
	class CmdBuffer;
	class Image : public DeviceObject {
	public:
		Image(Device& device) noexcept;
		~Image() noexcept;

		Image(const Image& buffer) = delete;
		Image& operator = (const Image& rhs) = delete;

		Image(Image&& rhs) noexcept : DeviceObject(rhs.device), image(rhs.image), imageType(rhs.imageType), imageFormat(rhs.imageFormat), width(rhs.width), height(rhs.height), depth(rhs.depth), vmaAllocation(rhs.vmaAllocation) { rhs.image = VK_NULL_HANDLE; }
		Image& operator = (Image&& rhs) noexcept {
			assert(&device == &rhs.GetDevice());
			image = rhs.image;
			imageType = rhs.imageType;
			imageFormat = rhs.imageFormat;
			width = rhs.width;
			height = rhs.height;
			depth = rhs.depth;
			vmaAllocation = rhs.vmaAllocation;
			rhs.image = VK_NULL_HANDLE;

			return *this;
		}

		static void lua_registar(LuaState lua);
		bool Initialize(VkImageType type, VkFormat format, uint32_t width, uint32_t height, uint32_t depth);
		void Uninitialize();
		Coroutine<bool> Upload(LuaState lua, Required<CmdBuffer*> cmdBuffer, std::string_view data);
		Coroutine<std::string> Download(LuaState lua, Required<CmdBuffer*> cmdBuffer);

		VkImageType GetImageType() const noexcept { return imageType; }
		VkFormat GetImageFormat() const noexcept { return imageFormat; }
		VkImage GetImage() const noexcept { return image; }
		VkImageView GetImageView() const noexcept { return imageView; }
		VkImageLayout GetImageLayout() const noexcept { return currentLayout; }
		void SetImageLayout(VkImageLayout imageLayout) noexcept { currentLayout = imageLayout; }

	protected:
		VkImage image = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
		VmaAllocation vmaAllocation = nullptr;
		VkImageType imageType = VK_IMAGE_TYPE_2D;
		VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
		VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;
		AsyncWorker::MemoryQuotaQueue::resource_t memoryQuotaResource;
	};
}

