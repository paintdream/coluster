// File.h
// PaintDream (paintdream@paintdream.com)
// 2023-04-02
//

#pragma once

#include "Storage.h"

namespace coluster {
	class Storage;
	class File;

	struct FileCompletion : public iris::iris_sync_t<Warp, AsyncWorker> {
		FileCompletion(const std::source_location& source, File& f);

		constexpr bool await_ready() const noexcept { return false; }
		void await_resume() noexcept;
		void await_suspend(CoroutineHandle<> handle);
		void Resume();

		Warp* warp;
		void* coroutineAddress;
		File& file;
		info_t info;
	};

	class File : public EnableReadWriteFence {
	public:
		File(Storage& storage) noexcept;
		~File() noexcept;

		enum class Status : uint8_t {
			Invalid,
			Ready,
			Reading,
			Writing,
		};

		static void lua_registar(LuaState lua);

		STORAGE_API Coroutine<bool> Open(std::string_view path, bool write);
		STORAGE_API Coroutine<size_t> Write(size_t offset, std::string_view data);
		STORAGE_API Coroutine<std::string_view> Read(size_t offset, size_t length);
		STORAGE_API void Flush();

		STORAGE_API uint64_t GetSize() const;
		STORAGE_API bool Close();
		STORAGE_API Storage& GetStorage() noexcept { return storage; }

#ifdef _WIN32
		static void CompleteOverlapped(void* overlapped);
#else
		static void CompleteURing(void* completion);
#endif

	protected:
		Storage& storage;
		std::string buffer;
		AsyncWorker::MemoryQuotaQueue::resource_t memoryQuotaResource;

#ifdef _WIN32
		void* fileHandle;
#else
		int fileFd;
#endif
		Status status = Status::Invalid;
	};
}

