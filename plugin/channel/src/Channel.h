// Channel.h
// PaintDream (paintdream@paintdream.com)
// 2023-02-19
//

#pragma once

#include "../../../src/Coluster.h"

#ifdef CHANNEL_EXPORT
	#ifdef __GNUC__
		#define CHANNEL_API __attribute__ ((visibility ("default")))
	#else
		#define CHANNEL_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#ifdef __GNUC__
		#define CHANNEL_API __attribute__ ((visibility ("default")))
	#else
		#define CHANNEL_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#endif

namespace coluster {
	class Channel : protected Warp, protected EnableReadWriteFence {
	public:
		Channel(AsyncWorker& asyncWorker) noexcept;
		~Channel() noexcept;

		static void lua_registar(LuaState lua);
		Warp& GetWarp() noexcept { return *this; }
		bool Setup(std::string_view protocol, std::string_view address);
		bool Connect(std::string_view address);
		bool Send(std::string_view data);
		std::string_view Recv();
		void Close() noexcept;

	protected:
		void FreeRecvBuffer() noexcept;

	private:
		int socket = -1;
		void* recvBuffer = nullptr;
	};
}
