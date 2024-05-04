// Coluster.h
// PaintDream (paintdream@paintdream.com)
// 2023-09-11
//

#include "../../../src/Coluster.h"

namespace coluster {
	template <typename K, typename V, template <typename...> typename MapTemplate>
	struct AsyncMap {
		using MapType = MapTemplate<K, V>;
		explicit AsyncMap(AsyncWorker& worker) : asyncWorker(worker) {
			//maps = std::vector<MapType>(asyncWorker.GetSharedWarps().size());
		}

		struct AwaitableGet : protected Warp::SwitchWarp {
			using Base = typename Warp::SwitchWarp;
			template <typename T>
			AwaitableGet(const std::source_location& source, MapType& map, Warp* target, T&& k) noexcept : Base(source, target, nullptr, true, false), asyncMap(map), key(std::forward<T>(k)) {}

			bool await_ready() const noexcept {
				return Base::await_ready();
			}

			void await_suspend(std::coroutine_handle<>&& handle) {
				Base::await_suspend(std::move(handle));
			}

			V* await_resume() noexcept {
				auto it = asyncMap.find(key);
				if (it != asyncMap.end()) {
					return &(it->second);
				} else {
					return nullptr;
				}
			}

			MapType& asyncMap;
			K key;
		};

		struct AwaitableSet : protected Warp::SwitchWarp {
			using Base = typename Warp::SwitchWarp;
			template <typename T, typename U>
			AwaitableSet(const std::source_location& source, MapType& map, Warp* target, T&& k, U&& u) noexcept : Base(source, target, nullptr, false, false), asyncMap(map), key(std::forward<T>(k)), value(std::forward<U>(u)) {}

			bool await_ready() const noexcept {
				return Base::await_ready();
			}

			void await_suspend(std::coroutine_handle<>&& handle) {
				Base::await_suspend(std::move(handle));
			}

			void await_resume() noexcept {
				asyncMap.emplace(std::move(key), std::move(value));
			}

			MapType& asyncMap;
			K key;
			V value;
		};

		template <typename T>
		size_t GetIndex(T&& key) const {
			return std::hash<std::remove_cvref_t<T>>()(key) % maps.size();
		}

		template <typename T>
		Warp* GetWarp(T&& key) const {
			return asyncWorker.GetSharedWarps()[GetIndex(std::forward<T>(key))].get();
		}

		template <typename T>
		AwaitableGet Get(T&& key) {
			size_t index = GetIndex(key);
			return AwaitableGet(std::source_location::current(), maps[index], asyncWorker.GetSharedWarps()[index].get(), std::forward<T>(key));
		}

		template <typename T, typename U>
		AwaitableSet Set(T&& key, U&& value) {
			size_t index = GetIndex(key);
			return AwaitableSet(std::source_location::current(), maps[index], asyncWorker.GetSharedWarps()[index].get(), std::forward<T>(key), std::forward<U>(value));
		}

	protected:
		AsyncWorker& asyncWorker;
		std::vector<MapType> maps;
	};
}