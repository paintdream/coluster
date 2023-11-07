// Coluster.h
// PaintDream (paintdream@paintdream.com)
// 2023-09-11
//

#include "Coluster.h"

namespace coluster {
	template <typename K, typename V, template <typename...> typename MapTemplate>
	struct AsyncMap {
		using MapType = MapTemplate<K, V>;
		explicit AsyncMap(AsyncWorker& worker) : asyncWorker(worker) {
			maps.resize(asyncWorker.GetSharedWarps().size());
		}

		struct Awaitable : protected Warp::SwitchWarp {
			using Base = typename Warp::SwitchWarp;
			template <typename T>
			Awaitable(const std::source_location& source, MapType& map, Warp* target, T&& k) noexcept : Base(source, target, nullptr), asyncMap(map), key(std::forward<T>(k)) {}

			bool await_ready() const noexcept {
				return Base::await_ready();
			}

			void await_suspend(std::coroutine_handle<>&& handle) {
				Base::await_suspend(std::move(handle));
			}

			V& await_resume() noexcept {
				return asyncMap[key];
			}

			MapType& asyncMap;
			K key;
		};

		size_t GetIndex(const K& key) const {
			return std::hash<std::remove_cvref_t<K>>()(key) % maps.size();
		}

		Warp* GetWarp(const K& key) const {
			return asyncWorker.GetSharedWarps()[GetIndex(key)].get();
		}

		template <typename T>
		Awaitable operator [](T&& key) {
			size_t index = GetIndex(key);
			return Awaitable(std::source_location::current(), maps[index], asyncWorker.GetSharedWarps()[index].get(), std::forward<T>(key));
		}

	protected:
		AsyncWorker& asyncWorker;
		std::vector<MapType> maps;
	};
}