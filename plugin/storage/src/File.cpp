#include "File.h"
#include "Storage.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <linux/io_uring.h> // use io_uring
static int sys_io_uring_enter(int ring_fd, unsigned int to_submit, unsigned int min_complete, unsigned int flags) {
	return (int)syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete, flags, NULL, 0);
}
#endif

namespace coluster {
	void File::lua_registar(LuaState lua) {
		lua.define<&File::Open>("Open");
		lua.define<&File::Read>("Read");
		lua.define<&File::Write>("Write");
		lua.define<&File::GetSize>("GetSize");
		lua.define<&File::Flush>("Flush");
		lua.define<&File::Close>("Close");
	}

	File::File(Storage& st) noexcept : storage(st),
#ifdef _WIN32
		fileHandle(nullptr)
#else
		fileFd(0)
#endif
	{}

	File::~File() noexcept {
		if (status != Status_Invalid) {
			Close();
		}
	}

	Coroutine<bool> File::Open(std::string_view path, bool write) {
		if (status != Status_Invalid) {
			fprintf(stderr, "[WARNING] File::OpenSync() -> Initializing twice takes no effects!\n");
			co_return false;
		}

		bool result = false;
		if (auto guard = write_fence()) {
			// go user space async io
			Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), static_cast<Warp*>(nullptr));

#ifdef _WIN32
			DWORD dwMinSize;
			dwMinSize = ::MultiByteToWideChar(CP_UTF8, 0, path.data(), (int)path.size(), nullptr, 0);
			std::wstring ret;
			ret.resize(dwMinSize + 1, 0);
			::MultiByteToWideChar(CP_UTF8, 0, path.data(), (int)path.size(), ret.data(), dwMinSize);
			fileHandle = ::CreateFileW(ret.c_str(), write ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ, nullptr, write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
			if (fileHandle != INVALID_HANDLE_VALUE) {
				HANDLE iocpHandle = ::CreateIoCompletionPort(fileHandle, storage.GetCompletionPort(), 0, 0);
				if (iocpHandle != nullptr) {
					result = true;
				} else {
					::CloseHandle(fileHandle);
					fileHandle = nullptr;
				}
			}
#else
			int fd = open(path.data(), write ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY, 0644);
			if (fd > 0) {
				fileFd = fd;
				result = true;
			}
#endif
			co_await Warp::Switch(std::source_location::current(), currentWarp);
		}

		if (result) {
			status = Status_Ready;
		}

		co_return std::move(result);
	}

	uint64_t File::GetSize() const {
		auto guard = read_fence();
#ifdef _WIN32
		LARGE_INTEGER li;
		if (::GetFileSizeEx(fileHandle, &li)) {
			return li.QuadPart;
		} else {
			return 0;
		}
#else
		struct stat sb;
		if (fstat(fileFd, &sb) != -1) {
			return sb.st_size;
		} else {
			return 0;
		}
#endif
	}

	FileCompletion::FileCompletion(const std::source_location& source, File& f) : iris_sync_t<Warp, AsyncWorker>(f.GetStorage().GetAsyncWorker()), warp(Warp::get_current_warp()), coroutineAddress(Warp::GetCurrentCoroutineAddress()), file(f) {
		Warp::ChainWait(source, warp, nullptr, nullptr);
		Warp::SetCurrentCoroutineAddress(nullptr);
	}

	void FileCompletion::await_suspend(CoroutineHandle<> handle) {
		info.handle = std::move(handle);
		info.warp = Warp::get_current_warp();
		file.GetStorage().DispatchOperation();
	}

	void FileCompletion::await_resume() noexcept {
		Warp::SetCurrentCoroutineAddress(coroutineAddress);
		Warp::ChainEnter(warp, nullptr, nullptr);
	}

	void FileCompletion::Resume() {
		dispatch(std::move(info));
	}

#ifdef _WIN32
	struct Overlapped : public OVERLAPPED {
		FileCompletion* completion;
	};

	void File::CompleteOverlapped(void* p) {
		reinterpret_cast<Overlapped*>(p)->completion->Resume();
	}
#else
	void File::CompleteURing(void* completion) {
		io_uring_cqe* cqe = reinterpret_cast<io_uring_cqe*>(completion);
		reinterpret_cast<FileCompletion*>(cqe->user_data)->Resume();
	}
#endif

	Coroutine<std::string_view> File::Read(size_t offset, size_t length) {
		if (status != Status_Ready || length == 0)
			co_return "";

		// allocate memory resource quota before allocating
		AsyncWorker& asyncWorker = storage.GetAsyncWorker();
		std::string_view result;
		if (auto guard = write_fence()) {
			status = Status_Reading;
			if (!storage.IsSupportAsyncIO()) {
				// go user space async io
				Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), static_cast<Warp*>(nullptr));

#ifdef _WIN32
				if (fileHandle != nullptr) {
					memoryQuotaResource = co_await asyncWorker.GetMemoryQuotaQueue().guard({ length, 0 });

					buffer.resize(length);
					DWORD bytes = 0;
					LARGE_INTEGER li;
					li.QuadPart = offset;
					::SetFilePointer(fileHandle, li.LowPart, &li.HighPart, FILE_BEGIN);
					if (::ReadFile(fileHandle, buffer.data(), iris::iris_verify_cast<DWORD>(length), &bytes, nullptr)) {
						result = std::string_view(buffer.data(), bytes);
					}
				}
#else
				if (fileFd > 0) {
					memoryQuotaResource = co_await asyncWorker.GetMemoryQuotaQueue().guard({ length, 0 });

					buffer.resize(length);
					if ((length = pread(fileFd, buffer.data(), length, offset)) != 0) {
						result = std::string_view(buffer.data(), length);
					}
				}
#endif
				co_await Warp::Switch(std::source_location::current(), currentWarp);
			} else {
				FileCompletion completion(std::source_location::current(), *this);

#ifdef _WIN32
				if (fileHandle != nullptr) {
					memoryQuotaResource = co_await asyncWorker.GetMemoryQuotaQueue().guard({ sizeof(Overlapped) + length, 0 });
					buffer.resize(sizeof(Overlapped) + length);
					Overlapped* overlapped = reinterpret_cast<Overlapped*>(buffer.data());
					memset(overlapped, 0, sizeof(Overlapped));

					LARGE_INTEGER li;
					li.QuadPart = offset;
					overlapped->Offset = li.LowPart;
					overlapped->OffsetHigh = li.HighPart;
					overlapped->completion = &completion;

					DWORD bytes = 0;
					auto* data = buffer.data() + sizeof(Overlapped);
					bool ret = ::ReadFile(fileHandle, data, iris::iris_verify_cast<DWORD>(length), &bytes, overlapped);
					if (ret || ::GetLastError() == ERROR_IO_PENDING) {
						result = std::string_view(data, length);
					}
				}
#else
				if (fileFd != 0) {
					memoryQuotaResource = co_await asyncWorker.GetMemoryQuotaQueue().guard({ sizeof(iovec) + length, 0 });
					buffer.resize(sizeof(iovec) + length);

					auto* data = buffer.data() + sizeof(iovec);
					iovec* v = reinterpret_cast<iovec*>(buffer.data());
					memset(v, 0, sizeof(iovec));
					v->iov_len = length;
					v->iov_base = data;

					Storage::URing& uring = storage.GetURing();

					uint32_t tail = *uring.submission.tail;
					if (((tail + 1 - *uring.submission.head) & *uring.submission.ring_mask) == 0) {
						// wait for at least one completion if full
						sys_io_uring_enter(uring.uringFd, 0, 1, IORING_ENTER_SQ_WAIT);
					}

					uint32_t index = tail & *uring.submission.ring_mask;
					io_uring_sqe& sqe = uring.submission.sqes[index];
					sqe.fd = fileFd;
					sqe.flags = 0;
					sqe.opcode = IORING_OP_READV;
					sqe.addr = reinterpret_cast<size_t>(v);
					sqe.len = 1;
					sqe.off = 0;
					sqe.user_data = reinterpret_cast<size_t>(&completion);
					uring.submission.array[index] = index;

					std::atomic_thread_fence(std::memory_order_release);
					*uring.submission.tail = tail + 1;

					std::atomic_thread_fence(std::memory_order_acquire);
					if (*uring.submission.flags & IORING_SQ_NEED_WAKEUP) {
						sys_io_uring_enter(uring.uringFd, 0, 0, IORING_ENTER_SQ_WAKEUP);
					}

					result = std::string_view(data, length);
				}
#endif

				if (result.size() == length) {
					Warp* currentWarp = Warp::get_current_warp();
					co_await completion; // wait for io completion
					co_await Warp::Switch(std::source_location::current(), currentWarp);
				}
			}
		}

		status = Status_Ready;
		co_return std::move(result);
	}

	Coroutine<size_t> File::Write(size_t offset, std::string_view input) {
		if (status != Status_Ready || input.size() == 0)
			co_return 0;

		status = Status_Writing;
		size_t result = 0;

		if (auto guard = write_fence()) {
			if (!storage.IsSupportAsyncIO()) {
				// go user space async io
				Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), static_cast<Warp*>(nullptr));

#ifdef _WIN32
				if (fileHandle != nullptr) {
					DWORD bytes = 0;
					LARGE_INTEGER li;
					li.QuadPart = input.size();
					::SetFilePointer(fileHandle, li.LowPart, &li.HighPart, FILE_BEGIN);

					if (::WriteFile(fileHandle, input.data(), iris::iris_verify_cast<DWORD>(input.size()), &bytes, nullptr)) {
						result = bytes;
					}
				}
#else
				if (fileFd > 0) {
					ssize_t len = pwrite(fileFd, input.data(), input.size(), offset);
					if (len < 0) len = 0;

					result = len;
				}
#endif
				co_await Warp::Switch(std::source_location::current(), currentWarp);
			} else {
				FileCompletion completion(std::source_location::current(), *this);
#ifdef _WIN32
				if (fileHandle != nullptr) {
					size_t size = input.size();
					buffer.resize(sizeof(Overlapped) + size);
					Overlapped* overlapped = reinterpret_cast<Overlapped*>(buffer.data());
					memset(overlapped, 0, sizeof(Overlapped));

					LARGE_INTEGER li;
					li.QuadPart = offset;
					overlapped->Offset = li.LowPart;
					overlapped->OffsetHigh = li.HighPart;
					overlapped->completion = &completion;

					DWORD bytes = 0;
					auto* data = buffer.data() + sizeof(Overlapped);
					memcpy(data, input.data(), size);

					bool ret = ::WriteFile(fileHandle, data, iris::iris_verify_cast<DWORD>(size), &bytes, overlapped);
					if (ret || ::GetLastError() == ERROR_IO_PENDING) {
						result = size;
					}
				}
#else
				if (fileFd != 0) {
					size_t size = input.size();
					buffer.resize(sizeof(iovec) + size);

					auto* data = buffer.data() + sizeof(iovec);
					memcpy(data, input.data(), size);

					iovec* v = reinterpret_cast<iovec*>(buffer.data());
					memset(v, 0, sizeof(iovec));
					v->iov_len = size;
					v->iov_base = data;

					Storage::URing& uring = storage.GetURing();

					uint32_t tail = *uring.submission.tail;
					if (((tail + 1 - *uring.submission.head) & *uring.submission.ring_mask) == 0) {
						// wait for at least one completion if full
						sys_io_uring_enter(uring.uringFd, 0, 1, IORING_ENTER_SQ_WAIT);
					}

					uint32_t index = tail & *uring.submission.ring_mask;
					io_uring_sqe& sqe = uring.submission.sqes[index];
					sqe.fd = fileFd;
					sqe.flags = 0;
					sqe.opcode = IORING_OP_WRITEV;
					sqe.addr = reinterpret_cast<size_t>(v);
					sqe.len = 1;
					sqe.off = 0;
					sqe.user_data = reinterpret_cast<size_t>(&completion);
					uring.submission.array[index] = index;

					std::atomic_thread_fence(std::memory_order_release);
					*uring.submission.tail = tail + 1;

					std::atomic_thread_fence(std::memory_order_acquire);
					if (*uring.submission.flags & IORING_SQ_NEED_WAKEUP) {
						sys_io_uring_enter(uring.uringFd, 0, 0, IORING_ENTER_SQ_WAKEUP);
					}

					result = size;
				}
#endif
				if (result != 0) {
					Warp* currentWarp = Warp::get_current_warp();
					co_await completion; // wait for io completion
					co_await Warp::Switch(std::source_location::current(), currentWarp);
				}
			}
		}

		status = Status_Ready;
		co_return std::move(result);
	}

	void File::Flush() {
		auto guard = write_fence();
		if (status != Status_Ready) {
			fprintf(stderr, "[WARNING] File::Flush() -> Not ready!\n");
			return;
		}

		buffer.clear();
		memoryQuotaResource.clear();

#ifdef _WIN32
		if (fileHandle != nullptr) {
			::FlushFileBuffers(fileHandle);
		}
#else
		if (fileFd > 0) {
			fsync(fileFd);
		}
#endif
	}

	bool File::Close() {
		auto guard = write_fence();
		if (status != Status_Ready) {
			fprintf(stderr, "[WARNING] File::Flush() -> Not ready!\n");
			return false;
		}

#ifdef _WIN32
		if (fileHandle != nullptr) {
			::CloseHandle(fileHandle);
			fileHandle = nullptr;
		}
#else
		if (fileFd > 0) {
			close(fileFd);
			fileFd = 0;
		}
#endif
		status = Status_Invalid;
		return true;
	}
}
