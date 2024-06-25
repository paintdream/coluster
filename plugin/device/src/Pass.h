// Pass.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-31
//

#pragma once

#include "Device.h"
#include "Device.h"
#include <string>

namespace coluster {
	class Device;
	class Shader;
	class Image;
	class Buffer;
	class CmdBuffer;

	class Pass : public DeviceObject {
	public:
		Pass(Device& device) noexcept;
		~Pass() noexcept;
		
		static void lua_registar(LuaState lua);
		void lua_finalize(LuaState lua, int index);
		Coroutine<Result<bool>> Initialize(Required<RefPtr<Shader>> s);
		Result<bool> BindImage(std::string_view name, Required<RefPtr<Image>> image);
		Result<bool> BindBuffer(std::string_view name, Required<RefPtr<Buffer>> buffer, size_t offset);
		Coroutine<Result<bool>> Dispatch(Required<CmdBuffer*> cmdBuffer, std::array<uint32_t, 3> dispatchCount);

		enum class Status {
			Invalid,
			Preparing,
			Executing,
			Completed
		};

		Status GetStatus() const noexcept { return status; }

	protected:
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		Status status = Status::Invalid;
		RefPtr<Shader> shader;
		std::vector<RefPtr<Image>> imageResources;
		std::vector<RefPtr<Buffer>> bufferResources;
		Device::DeviceQuotaQueue::resource_t quotaResource;
	};
}
