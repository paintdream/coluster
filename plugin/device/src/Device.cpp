#define VMA_IMPLEMENTATION
#include "../ref/vulkansdk/vk_mem_alloc.h"
#include "../ref/vulkansdk/Public/ShaderLang.h"

#include "Device.h"
#include "../../storage/src/Storage.h"
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <vector>

namespace coluster {
	static constexpr size_t MAX_DESCRIPTOR_COUNT = 1024u;

	SubmitCompletion::SubmitCompletion(const std::source_location& source, Device& dev, std::span<VkCommandBuffer> buffers) : iris_sync_t(dev.GetWarp().get_async_worker()), warp(Warp::get_current_warp()), coroutineAddress(GetCurrentCoroutineAddress()), device(dev), commandBuffers(buffers), fence(VK_NULL_HANDLE) {
		Warp::ChainWait(source, warp, nullptr, nullptr);
		SetCurrentCoroutineAddress(nullptr);
	}

	SubmitCompletion::~SubmitCompletion() {
		Clear();
	}

	void SubmitCompletion::Clear() {
		if (fence != VK_NULL_HANDLE) {
			device.FencePool::release(std::move(fence));
			fence = VK_NULL_HANDLE;
		}
	}

	void SubmitCompletion::await_suspend(CoroutineHandle<> handle) {
		info.handle = std::move(handle);
		info.warp = Warp::get_current_warp();
		device.QueueSubmitCompletionOnAny(*this);
	}

	void SubmitCompletion::await_resume() noexcept {
		SetCurrentCoroutineAddress(coroutineAddress);
		Warp::ChainEnter(warp, nullptr, nullptr);
	}

	void SubmitCompletion::Resume() {
		Clear();

		dispatch(std::move(info));
	}

	void Device::Verify(const char* message, VkResult res) noexcept {
		if (res != VK_SUCCESS) {
			fprintf(stderr, "[ERROR] Device::Verify() -> Unable to %s (debug).\n", message);
			assert(false);
		}
	}

#ifdef _DEBUG
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
		fprintf(stderr, "[DEBUG] DebugReportCallback() -> Vulkan debug: %d - %s\n", objectType, pMessage);
		assert(false);
		return VK_FALSE;
	}
#endif

	struct GLSLLangInitializer {
		GLSLLangInitializer() {
			glslang::InitializeProcess();
		}

		~GLSLLangInitializer() {
			glslang::FinalizeProcess();
		}
	};

	static std::once_flag glslLangOnce;
	Device::Device(AsyncWorker& asyncWorker, uint32_t maxDescriptorSetCount, uint32_t maxDescriptorCount) : Warp(asyncWorker), quota({ maxDescriptorSetCount, maxDescriptorCount, maxDescriptorCount, maxDescriptorCount }), quotaQueue(asyncWorker, quota) {
		static GLSLLangInitializer glslangInitializer;

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = "coluster";
		appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
		appInfo.pEngineName = "coluster";
		appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_1;

		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

		// Create Vulkan Instance
		std::vector<const char*> extensions = { "VK_KHR_get_physical_device_properties2" };

#ifdef _DEBUG 
		const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
		instanceCreateInfo.enabledLayerCount = 1;
		instanceCreateInfo.ppEnabledLayerNames = layers;
		// Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
		extensions.emplace_back("VK_EXT_debug_report");
#endif

		instanceCreateInfo.enabledExtensionCount = iris::iris_verify_cast<uint32_t>(extensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

		Verify("create vulkan instance", vkCreateInstance(&instanceCreateInfo, allocator, &instance));

#ifdef _DEBUG
		// Get the function pointer (required for any extensions)
		auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
		assert(vkCreateDebugReportCallbackEXT != nullptr);

		// Setup the debug report callback
		VkDebugReportCallbackCreateInfoEXT debugReport = {};
		debugReport.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debugReport.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		debugReport.pfnCallback = DebugReportCallback;
		debugReport.pUserData = nullptr;
		Verify("create debug report", vkCreateDebugReportCallbackEXT(instance, &debugReport, allocator, (VkDebugReportCallbackEXT*)&debugCallback));
#endif

		uint32_t gpuCount;
		VkResult err = vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
		if (err != VK_SUCCESS || gpuCount == 0) {
			return;
		}

		std::vector<VkPhysicalDevice> gpus(gpuCount);
		err = vkEnumeratePhysicalDevices(instance, &gpuCount, &gpus[0]);

		for (size_t i = 0; i < gpus.size(); i++) {
			VkPhysicalDevice physicalDevice = gpus[i];
			uint32_t count;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
			if (count == 0)
				continue;
			std::vector<VkQueueFamilyProperties> queues(count);

			uint32_t family = ~(uint32_t)0;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, &queues[0]);
			// prepare pure compute
			for (uint32_t j = count; j != 0; j--) {
				if (queues[j - 1].queueFlags & VK_QUEUE_COMPUTE_BIT) {
					family = j - 1;
					break;
				}
			}

			if (family == ~(uint32_t)0) {
				continue;
			}

			const float queuePriority[] = { 1.0f };

			VkDeviceQueueCreateInfo queueInfo[1] = {};
			queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo[0].queueFamilyIndex = family;
			queueInfo[0].queueCount = 1;
			queueInfo[0].pQueuePriorities = queuePriority;

			VkDeviceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.queueCreateInfoCount = sizeof(queueInfo) / sizeof(queueInfo[0]);
			createInfo.pQueueCreateInfos = queueInfo;

			Verify("create device", vkCreateDevice(physicalDevice, &createInfo, allocator, &device));
			vkGetDeviceQueue(device, family, 0, &queue);

			VmaAllocatorCreateInfo allocatorInfo = {};
			allocatorInfo.physicalDevice = physicalDevice;
			allocatorInfo.device = device;
			allocatorInfo.instance = instance;
			vmaCreateAllocator(&allocatorInfo, &vmaAllocator);

			VkDescriptorPoolSize descriptorTypes[] = {
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxDescriptorCount },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxDescriptorCount },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxDescriptorCount },
			};

			VkDescriptorPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			poolInfo.maxSets = maxDescriptorSetCount;
			poolInfo.poolSizeCount = sizeof(descriptorTypes) / sizeof(descriptorTypes[0]);
			poolInfo.pPoolSizes = descriptorTypes;
			Verify("create descriptor pool", vkCreateDescriptorPool(device, &poolInfo, allocator, &descriptorPool));

			VkPhysicalDeviceMemoryProperties memoryProps;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

			VkCommandPoolCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			info.queueFamilyIndex = family;
			Verify("create command pool", vkCreateCommandPool(device, &info, allocator, &mainCommandPool));

			break;
		}
	}

	Device::~Device() {
		while (queueingState.load(std::memory_order_acquire) != queue_state_idle) {
			queueingState.wait(queueingState.load(std::memory_order_relaxed), std::memory_order_acquire);
		}

		if (device != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(device);
			FencePool::clear();
			vkDestroyCommandPool(device, mainCommandPool, allocator);
			vkDestroyDescriptorPool(device, descriptorPool, allocator);

			vmaDestroyAllocator(vmaAllocator);
			vkDestroyDevice(device, allocator);
		}

		if (instance != VK_NULL_HANDLE) {
#ifdef _DEBUG
			PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
			vkDestroyDebugReportCallbackEXT(instance, (VkDebugReportCallbackEXT)debugCallback, allocator);
#endif

			vkDestroyInstance(instance, nullptr);
		}
	}

	template <>
	ResourceFence Device::acquire_element<ResourceFence>() {
		VkFence fence;
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = 0;
		fenceInfo.pNext = nullptr;

		Verify("create fence", vkCreateFence(device, &fenceInfo, allocator, &fence));
		return fence;
	}

	template <>
	void Device::release_element<ResourceFence>(ResourceFence&& fence) {
		vkDestroyFence(device, fence, allocator);
	}

	SubmitCompletion Device::SubmitCmdBuffers(const std::source_location& source, std::span<VkCommandBuffer> commandBuffers) {
		return SubmitCompletion(source, *this, commandBuffers);
	}

	void Device::QueueSubmitCompletionOnAny(SubmitCompletion& completion) {
		GetWarp().queue_routine([this, &completion]() {
			auto commandBuffers = completion.GetCommandBuffers();
			completion.SetFence(FencePool::acquire());

			VkSubmitInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			info.waitSemaphoreCount = 0;
			info.pWaitSemaphores = nullptr;
			info.pWaitDstStageMask = nullptr;
			info.commandBufferCount = iris::iris_verify_cast<uint32_t>(commandBuffers.size());
			info.pCommandBuffers = commandBuffers.data();
			info.signalSemaphoreCount = 0;
			info.pSignalSemaphores = nullptr;

			vkQueueSubmit(queue, 1, &info, completion.GetFence());
			requestSubmitCompletions.push(&completion);

			if (queueingState.exchange(queue_state_pending, std::memory_order_release) == queue_state_idle) {
				get_async_worker().queue([this]() { PollOnHelper(); }, Priority_Normal);
			}
		});
	}

	bool Device::TryDispatchSubmitCompletionOnHelper(SubmitCompletion& completion) {
		if (vkGetFenceStatus(device, completion.GetFence()) == VK_SUCCESS) [[unlikely]] {
			// signaled
			DispatchSubmitCompletionOnHelper(completion);
			return true;
		} else {
			return false;
		}
	}

	void Device::DispatchSubmitCompletionOnHelper(SubmitCompletion& completion) {
		VkFence fence = completion.GetFence();
		vkResetFences(device, 1, &fence);
		completion.Resume();
	}

	void Device::PollOnHelper() {
		// must not in any warps
		assert(Warp::get_current_warp() == nullptr);
		assert(queueingState.load(std::memory_order_acquire) == queue_state_pending);

		while (true) {
			queueingState.store(queue_state_executing, std::memory_order_release);

			while (!requestSubmitCompletions.empty()) {
				SubmitCompletion* completion = requestSubmitCompletions.top();
				requestSubmitCompletions.pop();

				if (!TryDispatchSubmitCompletionOnHelper(*completion)) {
					pollingSubmitCompletions.emplace_back(completion);
				}
			}

			pollingFences.clear();
			for (auto* completion : pollingSubmitCompletions) {
				pollingFences.emplace_back(completion->GetFence());
			}

			// go waiting
			if (!pollingFences.empty()) {
				if (vkWaitForFences(device, iris::iris_verify_cast<uint32_t>(pollingFences.size()), pollingFences.data(), VK_FALSE, ~(uint64_t)0) == VK_SUCCESS) {
					size_t j = 0;
					for (size_t i = 0; i < pollingSubmitCompletions.size(); i++) {
						if (!TryDispatchSubmitCompletionOnHelper(*pollingSubmitCompletions[i])) {
							pollingSubmitCompletions[j++] = pollingSubmitCompletions[i];
						}
					}

					pollingSubmitCompletions.resize(j);
				}
			}

			size_t expected = queue_state_executing;
			if (queueingState.compare_exchange_strong(expected, queue_state_idle, std::memory_order_release)) {
				queueingState.notify_one();
				break;
			}
		}
	}

	static const std::unordered_map<std::string_view, VkFormat> formatConstants = {
		DEFINE_MAP_ENTRY(FORMAT_R8_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R8_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R8G8_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8G8_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8G8_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R8G8_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8_SINT),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8_UINT),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8A8_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8A8_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8A8_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R8G8B8A8_SINT),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8A8_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8A8_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8A8_UINT),
		DEFINE_MAP_ENTRY(FORMAT_B8G8R8A8_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R16_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R16_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R16_SFLOAT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16G16_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16G16_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16_SFLOAT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16_SFLOAT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16A16_UNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16A16_SNORM),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16A16_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16A16_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R16G16B16A16_SFLOAT),
		DEFINE_MAP_ENTRY(FORMAT_R32_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R32_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R32_SFLOAT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32_SFLOAT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32B32_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32B32_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32B32_SFLOAT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32B32A32_UINT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32B32A32_SINT),
		DEFINE_MAP_ENTRY(FORMAT_R32G32B32A32_SFLOAT),
	};

	void Device::lua_registar(LuaState lua) {
		lua.set_current<&Device::Initialize>("Initialize");
		lua.set_current<&Device::TypeCmdBuffer>("TypeCmdBuffer");
		lua.set_current<&Device::TypeImage>("TypeImage");
		lua.set_current<&Device::TypeBuffer>("TypeBuffer");
		lua.set_current<&Device::TypeShader>("TypeShader");
		lua.set_current<&Device::TypePass>("TypePass");
		lua.set_current<&Device::TypeTexture>("TypeTexture");
		lua.set_current("Format", formatConstants);
	}

	void Device::lua_finalize(LuaState lua, int index) {
		get_async_worker().Synchronize(lua, this);
		lua.deref(std::move(storage));
	}

	void Device::Initialize(Required<RefPtr<Storage>> s) {
		storage = std::move(s.get());
	}
}

// implement for sub types
#include "CmdBuffer.h"
#include "Buffer.h"
#include "Image.h"
#include "Shader.h"
#include "Pass.h"
#include "Texture.h"

namespace coluster {
	Ref Device::TypeCmdBuffer(LuaState lua) {
		Ref type = lua.make_type<CmdBuffer>("CmdBuffer", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this()));
		return type;
	}

	Ref Device::TypeImage(LuaState lua) {
		Ref type = lua.make_type<Image>("Image", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this()));
		return type;
	}

	Ref Device::TypeBuffer(LuaState lua) {
		Ref type = lua.make_type<Buffer>("Buffer", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this()));
		return type;
	}

	Ref Device::TypeShader(LuaState lua) {
		Ref type = lua.make_type<Shader>("Shader", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this()));
		return type;
	}

	Ref Device::TypePass(LuaState lua) {
		Ref type = lua.make_type<Pass>("Pass", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this()));
		return type;
	}

	Ref Device::TypeTexture(LuaState lua) {
		if (!storage) {
			LuaState::log_error(lua.get_state(), "[ERROR] Device::TypeTexture() -> Must call Initialize() to pass storage instance before calling me (%p)!", this);
			return Ref();
		}

		Ref type = lua.make_type<Texture>("Texture", std::ref(*storage.get()));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this()));
		return type;
	}
}