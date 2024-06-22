// Texture.h
// PaintDream (paintdream@paintdream.com)
// 2023-04-02
//

#pragma once

#include "Device.h"

namespace coluster {
	class File;
	class CmdBuffer;
	class Image;
	class Storage;
	class Texture : public Object, protected EnableReadWriteFence {
	public:
		Texture(Storage& storage) noexcept;
		~Texture() noexcept override;

		enum class Status : uint8_t {
			Invalid,
			Ready,
			Reading,
			Writing,
			Uploading,
			Downloading
		};

		static void lua_registar(LuaState lua);
		Coroutine<bool> Load(LuaState lua, Required<File*> file);
		Coroutine<bool> Save(LuaState lua, Required<File*> file);
		Coroutine<bool> Upload(LuaState lua, Required<CmdBuffer*> cmdBuffer, Required<Image*> image);
		Coroutine<bool> Download(LuaState lua, Required<CmdBuffer*> cmdBuffer, Required<Image*> image);
		std::string_view GetPixels() const noexcept {
			return buffer;
		}

		bool SetPixels(std::string_view content, std::pair<uint32_t, uint32_t> resolution);
		std::pair<uint32_t, uint32_t> GetResolution() const noexcept {
			return resolution;
		}

	protected:
		Storage& storage;
		AsyncWorker::MemoryQuotaQueue::resource_t memoryQuotaResource;
		std::pair<uint32_t, uint32_t> resolution;
		std::string buffer;
		Status status = Status::Invalid;
	};
}


