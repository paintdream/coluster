#include "Pass.h"
#include "Device.h"
#include "Shader.h"
#include "CmdBuffer.h"
#include "Buffer.h"
#include "Image.h"

namespace coluster {
	Pass::Pass(Device& dev) noexcept : DeviceObject(dev) {}
	Pass::~Pass() noexcept {
		assert(descriptorSet == VK_NULL_HANDLE);
	}

	Coroutine<bool> Pass::Dispatch(Required<CmdBuffer*> cmdBuffer, std::array<uint32_t, 3> dispatchCount) {
		// Check object status
		if (!shader) {
			fprintf(stderr, "[ERROR] Pass::Dispatch() -> Invalid shader!\n");
			co_return false;
		}

		if (descriptorSet == VK_NULL_HANDLE) {
			fprintf(stderr, "[ERROR] Pass::Dispatch() -> Invalid descriptor set!\n");
			co_return false;
		}

		VkPipeline pipeline = shader->GetPipeline();
		VkPipelineLayout pipelineLayout = shader->GetPipelineLayout();

		if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
			fprintf(stderr, "[ERROR] Pass::Dispatch() -> Uninitialized shader pipeline!\n");
			co_return false;
		}

		VkCommandBuffer commandBuffer = cmdBuffer.get()->GetCommandBuffer();
		if (commandBuffer == VK_NULL_HANDLE) {
			fprintf(stderr, "[ERROR] Pass::Dispatch() -> Uninitialized command buffer!\n");
			co_return false;
		}

		// Schedule to command buffer
		status = Status_Executing;
		
		// add barriers for image & buffer
		std::vector<VkImageMemoryBarrier> imageBarriers;
		std::vector<VkBufferMemoryBarrier> bufferBarriers;
		imageBarriers.reserve(imageResources.size());
		bufferBarriers.reserve(bufferBarriers.size());

		for (auto& image : imageResources) {
			VkImageMemoryBarrier useBarrier = {};
			useBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			useBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			useBarrier.oldLayout = image->GetImageLayout();
			useBarrier.newLayout = image->GetImageLayout();
			useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.image = image->GetImage();

			useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			useBarrier.subresourceRange.baseMipLevel = 0;
			useBarrier.subresourceRange.levelCount = 1;
			useBarrier.subresourceRange.baseArrayLayer = 0;
			useBarrier.subresourceRange.layerCount = 1;

			imageBarriers.emplace_back(std::move(useBarrier));
		}

		for (auto& buffer : bufferResources) {
			VkBufferMemoryBarrier useBarrier = {};
			useBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			useBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			useBarrier.buffer = buffer->GetBuffer();
			useBarrier.offset = 0;
			useBarrier.size = buffer->GetBufferSize();

			bufferBarriers.emplace_back(std::move(useBarrier));
		}

		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &cmdBuffer.get()->GetWarp());

		vkCmdPipelineBarrier(cmdBuffer.get()->GetCommandBuffer(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
			iris::iris_verify_cast<uint32_t>(bufferBarriers.size()), bufferBarriers.data(), iris::iris_verify_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdDispatch(commandBuffer, dispatchCount[0], dispatchCount[1], dispatchCount[2]);

		cmdBuffer.get()->QueueCompletion([descriptorSet = std::exchange(descriptorSet, VK_NULL_HANDLE), quotaResourceAmount = quotaResource.move()](LuaState lua, CmdBuffer& cmdBuffer) mutable {
			auto& device = cmdBuffer.GetDevice();
			Device::DeviceQuotaQueue::resource_t quotaResource(device.GetQuotaQueue(), quotaResourceAmount);
			vkFreeDescriptorSets(device.GetDevice(), device.GetDescriptorPool(), 1, &descriptorSet);
		});

		cmdBuffer.get()->TransferReference(std::move(shader));

		for (auto& image : imageResources) {
			cmdBuffer.get()->TransferReference(std::move(image));
		}

		for (auto& buffer : bufferResources) {
			cmdBuffer.get()->TransferReference(std::move(buffer));
		}

		imageResources.clear();
		bufferResources.clear();

		// Do not wait here
		// co_await cmdBuffer.get()->WaitCompletion();
		co_await Warp::Switch(std::source_location::current(), currentWarp);

		status = Status_Completed;
		co_return true; // done!
	}

	bool Pass::BindImage(std::string_view name, Required<RefPtr<Image>> img) {
		if (!shader || status != Status_Preparing || descriptorSet == VK_NULL_HANDLE) {
			fprintf(stderr, "[ERROR] Pass::BindImage() -> Uninitialized Pass!\n");
			return false;
		}

		auto image = img.get().get();
		if (image->GetImageLayout() != VK_IMAGE_LAYOUT_GENERAL) {
			fprintf(stderr, "[ERROR] Pass::BindImage() -> Image is uninitialized!\n");
			return false;
		}

		// query binding
		uint32_t bindingPoint = shader->GetImageBindingPoint(name);
		if (bindingPoint == ~(uint32_t)0) {
			fprintf(stderr, "[ERROR] Pass::BindImage() -> Invalid binding point with given name %s!\n", name.data());
			return false;
		}

		VkDescriptorImageInfo info = {};
		info.imageLayout = image->GetImageLayout();
		info.imageView = image->GetImageView();

		VkWriteDescriptorSet desc = {};
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstSet = descriptorSet;
		desc.dstBinding = bindingPoint;
		desc.descriptorCount = 1;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		desc.pImageInfo = &info;

		vkUpdateDescriptorSets(device.GetDevice(), 1, &desc, 0, nullptr);
		imageResources.emplace_back(std::move(img.get()));

		return true;
	}

	bool Pass::BindBuffer(std::string_view name, Required<RefPtr<Buffer>> buf, size_t offset) {
		if (!shader || status != Status_Preparing || descriptorSet == VK_NULL_HANDLE) {
			fprintf(stderr, "[ERROR] Pass::BindBuffer() -> Uninitialized Pass!\n");
			return false;
		}

		// query binding
		uint32_t bindingPoint = shader->GetBufferBindingPoint(name);
		if (bindingPoint == ~(uint32_t)0) {
			fprintf(stderr, "[ERROR] Pass::BindBuffer() -> Invalid binding point with given name %s!\n", name.data());
			return false;
		}

		auto* buffer = buf.get().get();
		VkDescriptorBufferInfo info = {};
		info.buffer = buffer->GetBuffer();
		info.offset = offset;
		info.range = buffer->GetBufferSize() - std::min(buffer->GetBufferSize(), offset);

		VkWriteDescriptorSet desc = {};
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstSet = descriptorSet;
		desc.dstBinding = bindingPoint;
		desc.descriptorCount = 1;
		desc.descriptorType = buffer->GetUsage() & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc.pBufferInfo = &info;

		vkUpdateDescriptorSets(device.GetDevice(), 1, &desc, 0, nullptr);
		bufferResources.emplace_back(std::move(buf.get()));

		return true;
	}

	Coroutine<bool> Pass::Initialize(Required<RefPtr<Shader>> s) {
		if (descriptorSet != VK_NULL_HANDLE) {
			fprintf(stderr, "[WARNING] Pass::Intialize() -> Initializing twice takes no effects!\n");
			co_return false;
		}

		if (status != Status_Invalid) {
			fprintf(stderr, "[WARNING] Pass::Initialize() -> Pass status error!\n");
			co_return false;
		}
	
		assert(descriptorSet == VK_NULL_HANDLE);

		VkDescriptorSetLayout descriptorSetLayout = s.get()->GetDescriptorSetLayout();
		VkDescriptorSetAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &descriptorSetLayout;
		info.descriptorPool = device.GetDescriptorPool();

		size_t bufferCount = s.get()->GetBufferCount();
		size_t uniformBufferCount = s.get()->GetUniformBufferCount();
		assert(bufferCount >= uniformBufferCount);

		quotaResource = co_await device.GetQuotaQueue().guard({ (size_t)1, uniformBufferCount, bufferCount - uniformBufferCount, s.get()->GetImageCount() });
		device.Verify("create descriptor set", vkAllocateDescriptorSets(device.GetDevice(), &info, &descriptorSet));

		if (descriptorSet != VK_NULL_HANDLE) {
			shader = std::move(s.get());
			status = Status_Preparing;
			co_return true;
		} else {
			co_return false;
		}
	}

	void Pass::lua_finalize(LuaState lua, int index) {
		assert(status != Status_Executing);

		if (descriptorSet != VK_NULL_HANDLE) {
			vkFreeDescriptorSets(device.GetDevice(), device.GetDescriptorPool(), 1, &descriptorSet);
			descriptorSet = VK_NULL_HANDLE;
		}

		for (auto& res : imageResources) {
			lua.deref(std::move(res));
		}

		for (auto& res : bufferResources) {
			lua.deref(std::move(res));
		}

		if (shader) {
			lua.deref(std::move(shader));
		}

		status = Status_Invalid;
	}
	
	void Pass::lua_registar(LuaState lua) {
		lua.define<&Pass::Initialize>("Initialize");
		lua.define<&Pass::BindBuffer>("BindBuffer");
		lua.define<&Pass::BindImage>("BindImage");
		lua.define<&Pass::Dispatch>("Dispatch");
	}
}