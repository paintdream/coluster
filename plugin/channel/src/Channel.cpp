#include "Channel.h"
#include "../ref/nanomsg/src/nn.h"
#include "../ref/nanomsg/src/pubsub.h"
#include "../ref/nanomsg/src/pipeline.h"
#include "../ref/nanomsg/src/reqrep.h"

namespace coluster {
	Channel::Channel(AsyncWorker& asyncWorker) noexcept : Warp(asyncWorker) {}

	Channel::~Channel() noexcept {
		CloseImpl();
	}

	void Channel::CloseImpl() noexcept {
		if (socket != -1) {
			auto guard = write_fence();
			::nn_close(socket);
			socket = -1;
		}

		FreeRecvBuffer();
	}

	void Channel::lua_registar(LuaState lua) {
		lua.set_current<&Channel::Setup>("Setup");
		lua.set_current<&Channel::Connect>("Connect");
		lua.set_current<&Channel::Close>("Close");
		lua.set_current<&Channel::Send>("Send");
		lua.set_current<&Channel::Recv>("Recv");
	}

	Coroutine<void> Channel::Close() noexcept {
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		CloseImpl();
		co_await Warp::Switch(std::source_location::current(), currentWarp);
	}

	void Channel::FreeRecvBuffer() noexcept {
		if (recvBuffer != nullptr) {
			::nn_freemsg(recvBuffer);
			recvBuffer = nullptr;
		}
	}

	Coroutine<bool> Channel::Send(std::string_view data) {
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		bool ret = false;
		if (socket != -1) {
			auto guard = write_fence();
			ret = ::nn_send(socket, data.data(), data.length(), 0) >= 0;
		}

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		co_return std::move(ret);
	}

	Coroutine<std::string_view> Channel::Recv() {
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		FreeRecvBuffer();

		std::string_view ret;
		if (socket != -1) {
			auto guard = write_fence();
			void* buffer = nullptr;
			int bytes = ::nn_recv(socket, &buffer, NN_MSG, 0);
			if (bytes >= 0) {
				recvBuffer = buffer;
				ret = std::string_view(reinterpret_cast<char*>(buffer), bytes);
			}
		}

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		co_return std::move(ret);
	}

	Coroutine<bool> Channel::Connect(std::string_view address) {
		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		bool ret = false;
		if (socket != -1) {
			auto guard = write_fence();
			ret = ::nn_connect(socket, address.data()) >= 0;
		}

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		co_return std::move(ret);
	}

	Coroutine<Result<bool>> Channel::Setup(std::string_view protocol, std::string_view address) {
		// select protocol
		int protoindex = 0;
		if (protocol == "PUB") {
			protoindex = NN_PUB;
		} else if (protocol == "SUB") {
			protoindex = NN_SUB;
		} else if (protocol == "PUSH") {
			protoindex = NN_PUSH;
		} else if (protocol == "PULL") {
			protoindex = NN_PULL;
		} else if (protocol == "REQ") {
			protoindex = NN_REQ;
		} else if (protocol == "REP") {
			protoindex = NN_REP;
		} else {
			co_return ResultError("[ERROR] Channel::Setup() -> Unknown protocol.");
		}

		Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), &GetWarp());
		CloseImpl();

		auto guard = write_fence();


		bool ret = false;
		if (protoindex != 0) {
			auto guard = write_fence();
			socket = ::nn_socket(AF_SP, protoindex);
			// address must be zero-terminated
			if (socket >= 0 && ::nn_bind(socket, address.data()) >= 0) {
				ret = true;
			}
		}

		co_await Warp::Switch(std::source_location::current(), currentWarp);
		co_return std::move(ret);
	}
}
