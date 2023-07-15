// Storage.h
// PaintDream (paintdream@paintdream.com)
// 2023-04-02
//

#pragma once

#include "../../../src/Coluster.h"

#ifdef STORAGE_EXPORT
	#ifdef __GNUC__
		#define STORAGE_API __attribute__ ((visibility ("default")))
	#else
		#define STORAGE_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define STORAGE_API __attribute__ ((visibility ("default")))
	#else
		#define STORAGE_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif

#ifdef _WIN32
#else
struct io_uring_sqe;
struct io_uring_cqe;
#endif

namespace coluster {
	class File;
	class Texture;
	class Storage {
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
		~Storage() noexcept;

		static void lua_registar(LuaState lua);
		bool Initialize(std::string_view baseArchive);
		void Uninitialize();

		static Ref TypeFile(LuaState lua, Required<RefPtr<Storage>>&& self);
		static Ref TypeTexture(LuaState lua, Required<RefPtr<Storage>>&& self);
		static Ref TypeDatabase(LuaState lua, Required<RefPtr<Storage>>&& self);

		std::vector<std::string> ListDirectory(std::string_view path);
		bool MakeDirectory(std::string_view path);
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

