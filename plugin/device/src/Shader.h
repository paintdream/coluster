// Shader.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-31
//

#pragma once

#include "Device.h"
#include "Buffer.h"
#include <string>

namespace coluster {
	class Device;
	class Shader : public DeviceObject {
	public:
		Shader(Device& device) noexcept;
		~Shader() noexcept;
		
		static void lua_registar(LuaState lua);
		std::string Initialize(std::string_view shaderText, std::string_view entry);
		void Uninitialize();

		VkShaderModule GetShaderModule() const noexcept {
			return shaderModule;
		}

		VkPipeline GetPipeline() const noexcept {
			return pipeline;
		}

		VkPipelineLayout GetPipelineLayout() const noexcept {
			return pipelineLayout;
		}

		VkDescriptorSetLayout GetDescriptorSetLayout() const noexcept {
			return descriptorSetLayout;
		}

		size_t GetImageCount() const noexcept { return imageMaps.size(); }
		size_t GetBufferCount() const noexcept { return bufferLayoutMaps.size(); }
		size_t GetUniformBufferCount() const noexcept {
			size_t count = 0;
			for (const auto& bufferLayout : bufferLayoutMaps) {
				if (bufferLayout.second.isUniformBuffer) {
					count++;
				}
			}

			return count;
		}

		uint32_t GetImageBindingPoint(std::string_view imageName) const noexcept;
		uint32_t GetBufferBindingPoint(std::string_view bufferName) const noexcept;
		uint32_t GetBufferSize(std::string_view bufferName) const noexcept;
		std::array<uint32_t, 3> GetLocalSize() const noexcept { return localSize; }
		bool IsUniformBuffer(std::string_view bufferName) const noexcept;
		std::string FormatIntegers(std::vector<int32_t>&& buffer);
		std::string FormatFloats(std::vector<float>&& buffer);
		std::string FormatBuffer(std::string_view bufferName, std::vector<std::pair<std::string_view, std::string_view>>&& variables) const noexcept;
		bool IsBufferLayoutCompatible(std::string_view bufferName, Required<Shader*> rhsShader, std::string_view rhsBufferName) const noexcept;

		struct Variable {
			uint32_t offset;
			uint32_t size;
		};

		struct BufferLayout {
			uint32_t size = 0;
			uint32_t isUniformBuffer : 1 = 0;
			uint32_t bindingPoint : 31 = 0;
			std::vector<iris::iris_key_value_t<std::string_view, Variable>> variableMap;
		};

	protected:
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

		std::string allNameStrings;
		std::array<uint32_t, 3> localSize{ 1,1,1 };
		std::vector<iris::iris_key_value_t<std::string_view, uint32_t>> imageMaps;
		std::vector<iris::iris_key_value_t<std::string_view, BufferLayout>> bufferLayoutMaps;
	};
}
