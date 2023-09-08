// Device.h
// PaintDream (paintdream@paintdream.com)
// 2022-12-28
//

#pragma once

#include "Pipeline.h"

// forward declaration of VMA
typedef struct VmaAllocator_T* VmaAllocator;

namespace coluster {
	// awaitable completion of Queue
	class Device;
	class SubmitCompletion : public iris::iris_sync_t<Warp, AsyncWorker> {
	public:
		SubmitCompletion(const std::source_location& source, Device& worker, std::span<VkCommandBuffer> commandBuffers);
		~SubmitCompletion();
		SubmitCompletion(const SubmitCompletion&) = delete;
		SubmitCompletion(SubmitCompletion&&) = delete;

		// always suspended
		constexpr bool await_ready() const noexcept {
			return false;
		}

		void await_suspend(CoroutineHandle<> handle);
		void await_resume() noexcept;

		void Resume();
		VkFence GetFence() const noexcept { return fence; }
		void SetFence(VkFence f) noexcept { fence = f; }
		std::span<VkCommandBuffer> GetCommandBuffers() const noexcept { return commandBuffers; }

	protected:
		void Clear();

	protected:
		Warp* warp;
		void* coroutineAddress;
		Device& device;
		std::span<VkCommandBuffer> commandBuffers;
		VkFence fence;
		info_t info;
	};

	template <typename type, VkStructureType strutureType>
	struct Resource {
		Resource() {}
		Resource(type value) : resource(value) {}

		operator type () const noexcept {
			return resource;
		}

		Resource& operator = (const type& value) noexcept {
			resource = value.resource;
			return *this;
		}

	private:
		type resource;
	};

	using ResourceFence = Resource<VkFence, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO>;
	using FencePool = Pool<Device, ResourceFence>;

	class Storage;
	class Device : protected Warp, public FencePool {
	public:
		Device(AsyncWorker& asyncWorker, uint32_t maxDescriptorSetCount = 1024, uint32_t maxDescriptorCount = 4096);
		~Device();

		static void lua_registar(LuaState lua);
		void lua_finalize(LuaState lua, int index);

		// SubmitCmdBuffers() can be called from any warp
		SubmitCompletion SubmitCmdBuffers(const std::source_location& source, std::span<VkCommandBuffer> commandBuffers);
		void Initialize(Required<RefPtr<Storage>> storage);

		// Helper functions
		void Verify(const char* message, VkResult res) noexcept;
		VkDevice GetDevice() const noexcept { return device; }
		VkQueue GetQueue() const noexcept { return queue; }
		VmaAllocator GetVmaAllocator() const noexcept { return vmaAllocator; }
		VkAllocationCallbacks* GetAllocator() const noexcept { return allocator; }
		VkDescriptorPool GetDescriptorPool() const noexcept { return descriptorPool; }
		VkCommandPool GetCommandPool() const noexcept { return mainCommandPool; }
		Warp& GetWarp() noexcept { return *this; }
		AsyncWorker& GetAsyncWorker() noexcept { return get_async_worker(); }

		enum DeviceQuotaType {
			DeviceQuotaType_DescriptorSet,
			DeviceQuotaType_DescriptorUniform,
			DeviceQuotaType_DescriptorBuffer,
			DeviceQuotaType_DescriptorImage,
			DeviceQuotaType_Count
		};

		// descriptorSet, descriptorBuffer, descriptorImage
		using DeviceQuota = Quota<size_t, DeviceQuotaType_Count>;
		using DeviceQuotaQueue = QuotaQueue<DeviceQuota, Warp, AsyncWorker>;

		DeviceQuotaQueue& GetQuotaQueue() noexcept {
			return quotaQueue;
		}

	public:
		// acquire/release element defs
		template <typename element_t>
		[[nodiscard]] element_t acquire_element();

		template <typename element_t>
		void release_element(element_t&& element);

	protected:
		// for synchonization
		friend class SubmitCompletion;
		void PollOnHelper();

		// QueueSubmitCompletionOnAny can be called in any warps
		void QueueSubmitCompletionOnAny(SubmitCompletion& awaitable);
		bool TryDispatchSubmitCompletionOnHelper(SubmitCompletion& awaitable);
		void DispatchSubmitCompletionOnHelper(SubmitCompletion& awaitable);

		Ref TypeCmdBuffer(LuaState lua);
		Ref TypeImage(LuaState lua);
		Ref TypeBuffer(LuaState lua);
		Ref TypeShader(LuaState lua);
		Ref TypePass(LuaState lua);
		Ref TypeTexture(LuaState lua);

	protected:
		VkInstance instance = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		VkQueue queue = VK_NULL_HANDLE;
		VkCommandPool mainCommandPool = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkAllocationCallbacks* allocator = nullptr;
		VmaAllocator vmaAllocator = nullptr;
		RefPtr<Storage> storage;

		DeviceQuota quota;
		DeviceQuotaQueue quotaQueue;
		std::vector<VkFence> pollingFences;
		std::vector<SubmitCompletion*> pollingSubmitCompletions;
		QueueList<SubmitCompletion*> requestSubmitCompletions;
		std::atomic<size_t> queueingState = queue_state_idle;

#ifdef _DEBUG
		uint64_t debugCallback = 0;
#endif
	};
}

