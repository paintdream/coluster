// Storage.h
// PaintDream (paintdream@paintdream.com)
// 2023-04-02
//

#pragma once

#include "StorageCommon.h"

#ifdef _WIN32
#else
struct io_uring_sqe;
struct io_uring_cqe;
#endif

namespace coluster {
	class File;
	class Texture;
	class Storage : public Object {
	public:
#ifdef _WIN32
#else
		// reference: https://github.com/shuveb/io_uring-by-example
		struct URing {
			int uringFd;

			struct {
				uint32_t sqeSize;
				io_uring_sqe* sqes;
				uint32_t* head;
				uint32_t* tail;
				uint32_t* ring_mask;
				uint32_t* ring_entries;
				uint32_t* flags;
				uint32_t* array;
			} submission;
			struct {
				io_uring_cqe* cqes;
				uint32_t* head;
				uint32_t* tail;
				uint32_t* ring_mask;
				uint32_t* ring_entries;
			} completion;

			void* submissionMap;
			void* completionMap;
			uint32_t submissionSize;
			uint32_t completionSize;
		};
#endif

		Storage(AsyncWorker& asyncWorker);
		~Storage() noexcept override;

		static void lua_registar(LuaState lua);
		void lua_initialize(LuaState lua, int index);
		void lua_finalize(LuaState lua, int index);
		bool Initialize(std::string_view baseArchive);
		void Uninitialize();

		Ref TypeFile(LuaState lua);

		std::vector<std::string> ListDirectory(std::string_view path);
		bool Exists(std::string_view path);
		uint64_t GetFileSize(std::string_view path);
		void ResizeFile(std::string_view path, size_t newSize);
		bool CreateDirectories(std::string_view path);
		bool MakeDirectory(std::string_view path);
		bool IsDirectory(std::string_view path);
		bool Remove(std::string_view path);
		uint64_t RemoveAll(std::string_view path);
		void Rename(std::string_view oldName, std::string_view newName);
		void DispatchOperation();

		AsyncWorker& GetAsyncWorker() noexcept {
			return asyncWorker;
		}

		bool IsSupportAsyncIO() const noexcept {
			return supportAsyncIO;
		}

#ifdef _WIN32
		void* GetCompletionPort() const noexcept {
			return completionPort;
		}
#else
		URing& GetURing() noexcept {
			return uring;
		}
#endif

	protected:
		void Poll();

	protected:
		AsyncWorker& asyncWorker;
		std::string baseArchive;
		std::atomic<size_t> pendingOperations = 0;

#ifdef _WIN32
		void* completionPort;
#else
		URing uring;
#endif
		bool supportAsyncIO = true;
	};
}

