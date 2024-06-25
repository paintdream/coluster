#include "Texture.h"
#include "../../storage/src/File.h"
#include "../../storage/src/Storage.h"
#include "Image.h"
#include "CmdBuffer.h"
#include "webp/decode.h"
#include "webp/encode.h"

namespace coluster {
	void Texture::lua_registar(LuaState lua) {
		lua.set_current<&Texture::Load>("Load");
		lua.set_current<&Texture::Save>("Save");
		lua.set_current<&Texture::Upload>("Upload");
		lua.set_current<&Texture::Download>("Download");
		lua.set_current<&Texture::GetPixels>("GetPixels");
		lua.set_current<&Texture::SetPixels>("GetPixels");
		lua.set_current<&Texture::GetResolution>("GetResolution");
	}

	Texture::Texture(Storage& s) noexcept : storage(s) {}
	Texture::~Texture() noexcept {}
	
	Result<bool> Texture::SetPixels(std::string_view content, std::pair<uint32_t, uint32_t> res) {
		auto guard = write_fence();

		if (content.size() != res.first * res.second * 4) {
			return Result<bool>(std::nullopt, "[ERROR] Texture::SetPixels() -> Content size expected " + std::to_string(iris::iris_verify_cast<int>(res.first * res.second * 4)) + ", got " + std::to_string(iris::iris_verify_cast<int>(content.size())) + "");
			return false;
		}

		if (status != Status::Invalid && status != Status::Ready) {
			return Result<bool>(std::nullopt, "[ERROR] Texture::SetPixels() -> Invalid status!");
		}

		resolution = res;
		buffer = content;

		status = Status::Ready;
		return true;
	}

	Coroutine<Result<bool>> Texture::Load(LuaState lua, Required<File*> file) {
		uint8_t* decoded = nullptr;

		if (auto guard = write_fence()) {
			if (status != Status::Invalid) {
				co_return Result<bool>(std::nullopt, "[ERROR] Texture::Load() -> Invalid status!");
			}

			auto size = file.get()->GetSize();
			if (size == 0 || size >= std::numeric_limits<size_t>::max()) {
				co_return Result<bool>(std::nullopt, "[ERROR] Texture::Load() -> Invalid size!");
			}

			status = Status::Reading;
			Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), static_cast<Warp*>(nullptr));
			auto readData = co_await file.get()->Read(0, iris::iris_verify_cast<size_t>(size));
			assert(readData);
			std::string_view data = readData.value();
			int width, height;
			decoded = WebPDecodeRGBA(reinterpret_cast<const uint8_t*>(data.data()), data.size(), &width, &height);
			if (decoded != nullptr) {
				size_t length = width * height * 4;
				// allocate memory resource quota before allocating
				memoryQuotaResource = co_await storage.GetAsyncWorker().GetMemoryQuotaQueue().guard({length, 0});
				buffer.assign(reinterpret_cast<char*>(decoded), length);
				resolution = { width, height };
				WebPFree(decoded);

				status = Status::Ready;
			}

			co_await Warp::Switch(std::source_location::current(), currentWarp);
		}

		co_return decoded != nullptr;
	}

	Coroutine<Result<bool>> Texture::Save(LuaState lua, Required<File*> file) {
		bool ret = false;
		if (auto guard = write_fence()) {
			if (status != Status::Ready) {
				co_return Result<bool>(std::nullopt, "[ERROR] Texture::Save() -> Invalid status!");
			}
			
			status = Status::Writing;
			Warp* currentWarp = co_await Warp::Switch(std::source_location::current(), static_cast<Warp*>(nullptr));

			uint8_t* output = nullptr;
			size_t bytes = WebPEncodeLosslessRGBA(reinterpret_cast<uint8_t*>(buffer.data()), resolution.first, resolution.second, resolution.first * 4, &output);
			auto writeResult = co_await file.get()->Write(0, std::string_view(reinterpret_cast<char*>(output), bytes));
			WebPFree(output);
			status = Status::Ready;
			co_await Warp::Switch(std::source_location::current(), currentWarp);

			if (!writeResult) {
				co_return writeResult;
			}

			ret = writeResult.value() == bytes;
		}

		co_return std::move(ret);
	}

	Coroutine<Result<bool>> Texture::Upload(LuaState lua, Required<CmdBuffer*> cmdBuffer, Required<Image*> image) {
		bool success = false;
		if (auto guard = write_fence()) {
			status = Status::Uploading;
			if (image.get()->Initialize(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, resolution.first, resolution.second, 1)) {
				auto uploadResult = co_await image.get()->Upload(lua, std::move(cmdBuffer), buffer);
				status = Status::Ready;

				if (!uploadResult) {
					co_return uploadResult;
				}

				success = true;
			}
		}

		co_return std::move(success);
	}

	Coroutine<Result<bool>> Texture::Download(LuaState lua, Required<CmdBuffer*> cmdBuffer, Required<Image*> image) {
		bool success = false;
		if (auto guard = write_fence()) {
			status = Status::Uploading;
			size_t length = (size_t)resolution.first * resolution.second * 4;
			memoryQuotaResource = co_await storage.GetAsyncWorker().GetMemoryQuotaQueue().guard({length, 0});
			auto downloadResult = co_await image.get()->Download(lua, std::move(cmdBuffer));
			status = Status::Ready;

			if (!downloadResult) {
				co_return Result<bool>(std::nullopt, std::move(downloadResult.message));
			}

			buffer = std::move(downloadResult.value());
			success = !buffer.empty();
		}

		co_return std::move(success);
	}
}

