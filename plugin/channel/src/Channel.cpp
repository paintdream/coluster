#include "Channel.h"
#include "../ref/nanomsg/src/nn.h"
#include "../ref/nanomsg/src/pubsub.h"
#include "../ref/nanomsg/src/pipeline.h"
#include "../ref/nanomsg/src/reqrep.h"

namespace coluster {
	Channel::Channel(AsyncWorker& asyncWorker) noexcept : Warp(asyncWorker) {}

	Channel::~Channel() noexcept {
		Close();
	}

	void Channel::lua_registar(LuaState lua) {
		lua.define<&Channel::Setup>("Setup");
		lua.define<&Channel::Connect>("Connect");
		lua.define<&Channel::Close>("Close");
		lua.define<&Channel::Send>("Send");
		lua.define<&Channel::Recv>("Recv");
	}

	void Channel::Close() noexcept {
		auto guard = write_fence();
		if (socket != -1) {
			::nn_close(socket);
			socket = -1;
		}

		FreeRecvBuffer();
	}

	void Channel::FreeRecvBuffer() noexcept {
		if (recvBuffer != nullptr) {
			::nn_freemsg(recvBuffer);
			recvBuffer = nullptr;
		}
	}

	bool Channel::Send(std::string_view data) {
		auto guard = write_fence();
		if (socket != -1) {
			if (::nn_send(socket, data.data(), data.length(), 0) >= 0) {
				return true;
			}
		}

		return false;
	}

	std::string_view Channel::Recv() {
		auto guard = write_fence();
		FreeRecvBuffer();

		if (socket != -1) {
			void* buffer = nullptr;
			int bytes = ::nn_recv(socket, &buffer, NN_MSG, 0);
			if (bytes >= 0) {
				recvBuffer = buffer;
				return std::string_view(reinterpret_cast<char*>(buffer), bytes);
			}
		}

		return {};
	}

	bool Channel::Connect(std::string_view address) {
		if (socket != -1) {
			if (::nn_connect(socket, address.data()) >= 0) {
				return true;
			}
		}

		return false;
	}

	bool Channel::Setup(std::string_view protocol, std::string_view address) {
		Close();
		auto guard = write_fence();

		// select protocol
		int protoindex;
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
			// not a supported protocol
			return false;
		}

		socket = ::nn_socket(AF_SP, protoindex);
		if (socket < 0) {
			return false;
		}

		// address must be zero-terminated
		if (::nn_bind(socket, address.data()) < 0) {
			return false;
		}

		return true;
	}
}
