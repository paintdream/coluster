// Channel.h
// PaintDream (paintdream@paintdream.com)
// 2023-02-19
//

#pragma once

#include "ChannelCommon.h"

namespace coluster {
	class Channel : public Object, protected Warp, protected EnableReadWriteFence {
	public:
		Channel(AsyncWorker& asyncWorker) noexcept;
		~Channel() noexcept override;

		static void lua_registar(LuaState lua);
		Warp& GetWarp() noexcept { return *this; }
		Coroutine<Result<bool>> Setup(std::string_view protocol, std::string_view address);
		Coroutine<bool> Connect(std::string_view address);
		Coroutine<bool> Send(std::string_view data);
		Coroutine<std::string_view> Recv();
		Coroutine<void> Close() noexcept;

	protected:
		void FreeRecvBuffer() noexcept;
		void CloseImpl() noexcept;

	private:
		int socket = -1;
		void* recvBuffer = nullptr;
	};
}
