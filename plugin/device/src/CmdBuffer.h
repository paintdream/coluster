// CmdBuffer.h
// PaintDream (paintdream@paintdream.com)
// 2023-1-2
//

#pragma once

#include "Device.h"

namespace coluster {
	// CmdBuffer for warp management
	class CmdBuffer : public DeviceObject, protected Warp {
	public:
		CmdBuffer(Device& device);
		~CmdBuffer() noexcept;
	
		// Script interfaces
		static void lua_registar(LuaState lua);
		void lua_finalize(LuaState lua, int index);

		VkCommandBuffer GetCommandBuffer() const noexcept { return commandBuffers[0]; }
		Warp& GetWarp() noexcept { return *this; }

		void Begin() noexcept;
		void End() noexcept;

		enum class EncodeState : uint32_t {
			Ready = 0,
			Recording,
			Complete,
		};

		enum class SubmitState : uint32_t {
			Idle = 0,
			Queueing,
		};

		EncodeState GetState() const noexcept { return encodeState; }
		Coroutine<void> Submit(LuaState lua);
		void QueueCompletion(std::function<void(LuaState lua, CmdBuffer&)>&& completion);
		void TransferReference(Ref&& ref);

	protected:
		void Cleanup(LuaState lua);
		
	protected:
		// we use double buffers to avoid blocking recording while executing
		VkCommandBuffer commandBuffers[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
		VkFence executeFence = VK_NULL_HANDLE;
		EncodeState encodeState = EncodeState::Ready;
		SubmitState submitState = SubmitState::Idle;
		std::vector<std::function<void(LuaState, CmdBuffer&)>> completions;
		std::vector<Ref> transferredReferences;
	};
}

