#include "CmdBuffer.h"
#include "Device.h"

namespace coluster {
	class CmdCompletion : public iris::iris_sync_t<Warp, AsyncWorker> {
	public:
		CmdCompletion(CmdBuffer& cmdBuffer);
		~CmdCompletion();
		CmdCompletion(const CmdCompletion&) = delete;
		CmdCompletion(CmdCompletion&&) = delete;

		// always suspended
		constexpr bool await_ready() const noexcept { return false; }
		void await_suspend(CoroutineHandle<> handle);
		constexpr void await_resume() noexcept {}

	protected:
		CmdBuffer& cmdBuffer;
		info_t info;
	};

	CmdCompletion::CmdCompletion(CmdBuffer& buffer) : iris_sync_t(buffer.GetWarp().get_async_worker()), cmdBuffer(buffer) {}
	CmdCompletion::~CmdCompletion() {}

	void CmdCompletion::await_suspend(CoroutineHandle<> handle) {
		info.handle = std::move(handle);
		info.warp = Warp::get_current_warp();
		
		cmdBuffer.QueueCompletion([this](LuaState lua, CmdBuffer& cmdBuffer) {
			dispatch(std::move(info));
		});
	}

	CmdBuffer::CmdBuffer(Device& dev) : DeviceObject(dev), Warp(dev.GetWarp().get_async_worker()) {
		// must construct on device warp
		VkCommandBufferAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.commandBufferCount = sizeof(commandBuffers) / sizeof(commandBuffers[0]);
		info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		info.commandPool = device.GetCommandPool();
		info.pNext = nullptr;

		device.Verify("allocate command buffer", vkAllocateCommandBuffers(device.GetDevice(), &info, commandBuffers));
		Begin();
	}

	CmdBuffer::~CmdBuffer() noexcept {
		End();
		assert(encodeState == EncodeState::Ready || encodeState == EncodeState::Complete);
		vkFreeCommandBuffers(device.GetDevice(), device.GetCommandPool(), sizeof(commandBuffers) / sizeof(commandBuffers[0]), commandBuffers);
	}

	void CmdBuffer::Begin() noexcept {
		assert(encodeState == EncodeState::Ready);

		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(commandBuffers[0], &info);
		encodeState = EncodeState::Recording;
	}

	void CmdBuffer::End() noexcept {
		assert(encodeState == EncodeState::Recording);
		vkEndCommandBuffer(commandBuffers[0]);
		encodeState = EncodeState::Complete;
	}

	Coroutine<void> CmdBuffer::Submit(LuaState lua) {
		Warp* currentWarp = co_await Warp::Switch(&GetWarp());
		// already queued
		while (true) {
			if (submitState == SubmitState::Idle) {
				// OK, go submitting
				End();
				std::swap(commandBuffers[0], commandBuffers[1]);
				encodeState = EncodeState::Ready;
				Begin();

				submitState = SubmitState::Queueing;
				co_await device.SubmitCmdBuffers({ &commandBuffers[1], 1 });
				submitState = SubmitState::Idle;
				break;
			} else {
				// submitted command buffer busy, keep recording
				co_await CmdCompletion(*this);
				co_await Warp::Switch(&GetWarp());
			}
		}

		co_await Warp::Switch(currentWarp);
		Cleanup(lua);
	}

	void CmdBuffer::Cleanup(LuaState lua) {
		std::vector<std::function<void(LuaState, CmdBuffer&)>> handlers = std::move(completions);
		completions.clear();
		std::vector<Ref> references = std::move(transferredReferences);
		transferredReferences.clear();

		for (auto&& func : handlers) {
			func(lua, *this);
		}

		for (auto& ref : references) {
			lua.deref(std::move(ref));
		}
	}

	void CmdBuffer::lua_finalize(LuaState lua, int index) {
		Cleanup(lua);
	}

	void CmdBuffer::QueueCompletion(std::function<void(LuaState, CmdBuffer&)>&& completion) {
		GetWarp().validate();
		completions.emplace_back(std::move(completion));
	}

	void CmdBuffer::TransferReference(Ref&& ref) {
		GetWarp().validate();
		transferredReferences.emplace_back(std::move(ref));
	}

	void CmdBuffer::lua_registar(LuaState lua) {
		lua.define<&CmdBuffer::Submit>("Submit");
	}
}
