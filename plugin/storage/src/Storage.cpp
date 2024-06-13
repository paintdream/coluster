#include "Storage.h"
#include "File.h"
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <linux/io_uring.h>
static constexpr uint32_t URING_QUEUE_DEPTH = 1;
static int sys_io_uring_setup(uint32_t entries, struct io_uring_params* p) {
	return (int)syscall(__NR_io_uring_setup, entries, p);
}
#endif

namespace coluster {
	Storage::Storage(AsyncWorker& s) : asyncWorker(s) {
#ifdef _WIN32
		completionPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, reinterpret_cast<ULONG_PTR>(this), 1);
#else
		io_uring_params p;
		memset(&p, 0, sizeof(p));
		p.flags |= IORING_SETUP_SQPOLL;
		uring.uringFd = sys_io_uring_setup(URING_QUEUE_DEPTH, &p);
		if (uring.uringFd < 0) {
			supportAsyncIO = false;
			return;
		}

		int submissionSize = p.sq_off.array + p.sq_entries * sizeof(uint32_t);
		int completionSize = p.cq_off.cqes + p.cq_entries * sizeof(io_uring_cqe);

		if (p.features & IORING_FEAT_SINGLE_MMAP) {
			completionSize = submissionSize = std::max(completionSize, submissionSize);
		}

		uint8_t* submissionMap = reinterpret_cast<uint8_t*>(mmap(0, submissionSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, uring.uringFd, IORING_OFF_SQ_RING));
		uint8_t* completionMap = nullptr;

		if (p.features & IORING_FEAT_SINGLE_MMAP) {
			completionMap = submissionMap;
		} else {
			completionMap = reinterpret_cast<uint8_t*>(mmap(0, completionSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, uring.uringFd, IORING_OFF_CQ_RING));
		}

		uring.submission.head = reinterpret_cast<uint32_t*>(submissionMap + p.sq_off.head);
		uring.submission.tail = reinterpret_cast<uint32_t*>(submissionMap + p.sq_off.tail);
		uring.submission.ring_mask = reinterpret_cast<uint32_t*>(submissionMap + p.sq_off.ring_mask);
		uring.submission.ring_entries = reinterpret_cast<uint32_t*>(submissionMap + p.sq_off.ring_entries);
		uring.submission.flags = reinterpret_cast<uint32_t*>(submissionMap + p.sq_off.flags);
		uring.submission.array = reinterpret_cast<uint32_t*>(submissionMap + p.sq_off.array);
		uring.submission.sqeSize = p.sq_entries * sizeof(io_uring_sqe);
		uring.submission.sqes = reinterpret_cast<io_uring_sqe*>(mmap(0, uring.submission.sqeSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, uring.uringFd, IORING_OFF_SQES));

		uring.completion.head = reinterpret_cast<uint32_t*>(completionMap + p.cq_off.head);
		uring.completion.tail = reinterpret_cast<uint32_t*>(completionMap + p.cq_off.tail);
		uring.completion.ring_mask = reinterpret_cast<uint32_t*>(completionMap + p.cq_off.ring_mask);
		uring.completion.ring_entries = reinterpret_cast<uint32_t*>(completionMap + p.cq_off.ring_entries);
		uring.completion.cqes = reinterpret_cast<io_uring_cqe*>(completionMap + p.cq_off.cqes);

		uring.submissionSize = submissionSize;
		uring.completionSize = completionSize;
		uring.submissionMap = submissionMap;
		uring.completionMap = completionMap;
#endif
	}

	Storage::~Storage() noexcept {
#ifdef _WIN32
		::CloseHandle(completionPort);
#else
		if (uring.uringFd >= 0) {
			munmap(uring.submission.sqes, uring.submission.sqeSize);
			munmap(uring.submissionMap, uring.submissionSize);
			if (uring.completionMap != uring.submissionMap) {
				munmap(uring.completionMap, uring.completionSize);
			}

			close(uring.uringFd);
		}
#endif
	}

	void Storage::lua_initialize(LuaState lua, int index) {

	}

	void Storage::lua_finalize(LuaState lua, int index) {
		asyncWorker.Synchronize(lua, nullptr);
	}

	void Storage::lua_registar(LuaState lua) {
		lua.set_current<&Storage::TypeFile>("TypeFile");
		lua.set_current<&Storage::MakeDirectory>("MakeDirectory");
		lua.set_current<&Storage::ListDirectory>("ListDirectory");
		lua.set_current<&Storage::CreateDirectories>("CreateDirectories");
		lua.set_current<&Storage::GetFileSize>("GetFileSize");
		lua.set_current<&Storage::ResizeFile>("ResizeFile");
		lua.set_current<&Storage::Exists>("Exists");
		lua.set_current<&Storage::IsDirectory>("IsDirectory");
		lua.set_current<&Storage::Remove>("Remove");
		lua.set_current<&Storage::RemoveAll>("RemoveAll");
		lua.set_current<&Storage::Rename>("Rename");
	}

	uint64_t Storage::GetFileSize(std::string_view path) {
		return std::filesystem::file_size(path);
	}

	void Storage::ResizeFile(std::string_view path, size_t newSize) {
		return std::filesystem::resize_file(path, newSize);
	}

	bool Storage::Exists(std::string_view path) {
		return std::filesystem::exists(path);
	}

	bool Storage::CreateDirectories(std::string_view path) {
		return std::filesystem::create_directories(path);
	}

	bool Storage::MakeDirectory(std::string_view path) {
		return std::filesystem::create_directories(path);
	}

	bool Storage::IsDirectory(std::string_view path) {
		return std::filesystem::is_directory(path);
	}

	bool Storage::Remove(std::string_view path) {
		return std::filesystem::remove(path);
	}

	uint64_t Storage::RemoveAll(std::string_view path) {
		return std::filesystem::remove_all(path);
	}

	void Storage::Rename(std::string_view oldName, std::string_view newName) {
		return std::filesystem::rename(oldName, newName);
	}

	std::vector<std::string> Storage::ListDirectory(std::string_view path) {
		std::vector<std::string> result;
		if (std::filesystem::is_directory(path)) {
			for (auto&& entry : std::filesystem::directory_iterator(path)) {
				if (entry.is_directory()) {
					result.emplace_back(entry.path().filename().string() + "/");
				} else {
					result.emplace_back(entry.path().filename().string());
				}
			}
		}

		return result;
	}

	bool Storage::Initialize(std::string_view path) {
		baseArchive = path;
		return true;
	}

	void Storage::Uninitialize() {

	}

	Ref Storage::TypeFile(LuaState lua) {
		Ref type = lua.make_type<File>("File", std::ref(*this));
		type.set(lua, "__host", lua.get_context<Ref>(LuaState::context_this_t()));
		return type;
	}

	void Storage::DispatchOperation() {
		assert(supportAsyncIO);
		if (pendingOperations.fetch_add(1, std::memory_order_release) == 0) {
			asyncWorker.queue([this]() {
				Poll();
			}, Priority_Lowest);
		}
	}

	void Storage::Poll() {
#ifdef _WIN32
		DWORD interval = INFINITE;
		DWORD bytes = 0;
		ULONG_PTR key = 0;
		LPOVERLAPPED overlapped = nullptr;

		while (::GetQueuedCompletionStatus(completionPort, &bytes, &key, &overlapped, interval)) {
			if (overlapped != nullptr) {
				File::CompleteOverlapped(overlapped);
				// no more pending operations
				if (pendingOperations.fetch_sub(1, std::memory_order_release) == 1) {
					break;
				}
			} else {
				break;
			}
		}
#else
		uint32_t head = *uring.completion.head;
		uint32_t ring_mask = *uring.completion.ring_mask;
		auto& asyncWorker = GetAsyncWorker();

		while (true) {
			if (head != *uring.completion.tail) {
				File::CompleteURing(&uring.completion.cqes[head & ring_mask]);
				head++;

				std::atomic_thread_fence(std::memory_order_release);
				*uring.completion.head = head;

				if (pendingOperations.fetch_sub(1, std::memory_order_acquire) == 1) {
					break;
				}
			} else {
				asyncWorker.poll_delay(Priority_Highest, std::chrono::milliseconds(20));
			}
		}
#endif
	}
}
