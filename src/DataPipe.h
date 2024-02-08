// DataPipe.h
// PaintDream (paintdream@paintdream.com)
// 2024-1-5
//

#pragma once

#include "Coluster.h"

namespace coluster {
	class DataPipe : public Object, protected EnableInOutFence {
	public:
		DataPipe(AsyncWorker& asyncWorker);
		~DataPipe() noexcept override;
		static void lua_registar(LuaState lua);
		void lua_initialize(LuaState lua, int index) noexcept;

		bool BindInputWarp(Warp* warp) noexcept;
		bool BindOutputWarp(Warp* warp) noexcept;

		void Push(std::string_view data);
		Coroutine<std::string> Pop();
		bool Empty() const noexcept;

	protected:
		template <bool input>
		struct RequiredDataPipe : public LuaState::required_base_t {
			using required_type_t = DataPipe*;
			RequiredDataPipe(DataPipe* v) noexcept : value(v) {}
			required_type_t operator -> () const noexcept {
				return value;
			}

			operator required_type_t () const noexcept {
				if (value != nullptr) {
					if constexpr (input) {
						if (value->inputWarp == Warp::get_current_warp()) {
							return value;
						}
					} else {
						if (value->outputWarp == Warp::get_current_warp()) {
							return value;
						}
					}
				}

				return nullptr;
			}

		protected:
			DataPipe* value = nullptr;
		};

		static void CheckedPush(RequiredDataPipe<true>&& self, std::string_view data);
		static Coroutine<std::string> CheckedPop(RequiredDataPipe<false>&& self);
		static bool CheckedEmpty(RequiredDataPipe<false>&& self);

	protected:
		AsyncPipe<size_t> asyncPipe;
		QueueList<uint8_t> dataQueueList;
		Warp* inputWarp = nullptr;
		Warp* outputWarp = nullptr;
	};
}
