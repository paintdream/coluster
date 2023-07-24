#include "Texture.h"
#include "../../storage/src/File.h"
#include "../../storage/src/Storage.h"
#include "Image.h"
#include "CmdBuffer.h"
#include "webp/decode.h"
#include "webp/encode.h"

namespace coluster {
	void Texture::lua_registar(LuaState lua) {
		lua.define<&Texture::Load>("Load");
		lua.define<&Texture::Save>("Save");
		lua.define<&Texture::Upload>("Upload");
		lua.define<&Texture::Download>("Download");
		lua.define<&Texture::GetPixels>("GetPixels");
		lua.define<&Texture::SetPixels>("GetPixels");
		lua.define<&Texture::GetResolution>("GetResolution");
	}

	Texture::Texture(Storage& s) noexcept : storage(s) {}
	Texture::~Texture() noexcept {}
	
	bool Texture::SetPixels(std::string_view content, std::pair<uint32_t, uint32_t> res) {
		auto guard = write_fence();

		if (content.size() != res.first * res.second * 4) {
			fprintf(stderr, "[ERROR] Texture::SetPixels() -> Content size expected %d, got %d\n", iris::iris_verify_cast<int>(res.first * res.second * 4), iris::iris_verify_cast<int>(content.size()));
			return false;
		}

		if (status != Status_Invalid && status != Status_Ready) {
			fprintf(stderr, "[ERROR] Texture::SetPixels() -> Invalid status!\n");
			return false;
		}

		resolution = res;
		buffer = content;

		status = Status_Ready;
		return true;
	}

	Coroutine<bool> Texture::Load(LuaState lua, Required<File*> file) {
		uint8_t* decoded = nullptr;

		if (auto guard = write_fence()) {
			if (status != Status_Invalid) {
				fprintf(stderr, "[ERROR] Texture::Load() -> Invalid status!\n");
				co_return false;
			}

			auto size = file.get()->GetSize();
			if (size == 0 || size >= std::numeric_limits<size_t>::max()) {
				fprintf(stderr, "[ERROR] Texture::Load() -> Invalid size!\n");
				co_return false;
			}

			status = Status_Reading;
			Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), static_cast<Warp*>(nullptr));
			std::string_view data = co_await file.get()->Read(0, iris::iris_verify_cast<size_t>(size));
			int width, height;
			decoded = WebPDecodeRGBA(reinterpret_cast<const uint8_t*>(data.data()), data.size(), &width, &height);
			if (decoded != nullptr) {
				size_t length = width * height * 4;
				// allocate memory resource quota before allocating
				memoryQuotaResource = co_await storage.GetAsyncWorker().GetMemoryQuotaQueue().guard({length, 0});
				buffer.assign(reinterpret_cast<char*>(decoded), length);
				resolution = { width, height };
				WebPFree(decoded);

				status = Status_Ready;
			} else {
				fprintf(stderr, "[ERROR] Texture::Load() -> Decode failed!\n");
			}

			co_await Warp::Switch(std::source_location::current(), currentWarp);
		}

		co_return decoded != nullptr;
	}

	Coroutine<bool> Texture::Save(LuaState lua, Required<File*> file) {
		bool ret = false;
		if (auto guard = write_fence()) {
			if (status != Status_Ready) {
				fprintf(stderr, "[ERROR] Texture::Save() -> Invalid status!\n");
				co_return false;
			}
			
			status = Status_Writing;
			Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), static_cast<Warp*>(nullptr));

			uint8_t* output = nullptr;
			size_t bytes = WebPEncodeLosslessRGBA(reinterpret_cast<uint8_t*>(buffer.data()), resolution.first, resolution.second, resolution.first * 4, &output);
			size_t writeBytes = co_await file.get()->Write(0, std::string_view(reinterpret_cast<char*>(output), bytes));
			WebPFree(output);

			status = Status_Ready;
			co_await Warp::Switch(std::source_location::current(), currentWarp);

			ret = writeBytes == bytes;
		}

		co_return std::move(ret);
	}

	Coroutine<bool> Texture::Upload(LuaState lua, Required<CmdBuffer*> cmdBuffer, Required<Image*> image) {
		bool success = false;
		if (auto guard = write_fence()) {
			status = Status_Uploading;
			if (image.get()->Initialize(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, resolution.first, resolution.second, 1)) {
				success = co_await image.get()->Upload(lua, std::move(cmdBuffer), buffer);
			}

			status = Status_Ready;
		}

		co_return std::move(success);
	}

	Coroutine<bool> Texture::Download(LuaState lua, Required<CmdBuffer*> cmdBuffer, Required<Image*> image) {
		bool success = false;
		if (auto guard = write_fence()) {
			status = Status_Uploading;
			size_t length = (size_t)resolution.first * resolution.second * 4;
			memoryQuotaResource = co_await storage.GetAsyncWorker().GetMemoryQuotaQueue().guard({length, 0});
			buffer = co_await image.get()->Download(lua, std::move(cmdBuffer));
			status = Status_Ready;
			success = !buffer.empty();
		}

		co_return std::move(success);
	}
}

