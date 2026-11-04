#pragma once
#include <filesystem>
#include <future>
#include <string>

#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Utilities/Memory.h"

namespace SV
{
	// Directory the planet texture paths are relative to (SV_DEFAULT_DATA_DIR unless overridden)
	void SetTextureDirectory(const std::filesystem::path& directory);
	const std::filesystem::path& GetTextureDirectory();

	// A texture whose image is decoded on a background thread. Call Poll once a frame from the
	// render thread: when decoding has finished it uploads the image (with mip maps) and returns true.
	class AsyncTexture
	{
	public:
		// relativePath is relative to the texture directory
		explicit AsyncTexture(const std::string& relativePath);

		// Returns true on the call that makes the texture available
		bool Poll();

		[[nodiscard]] bool IsReady() const { return texture != nullptr; }
		// nullptr until the texture is ready
		[[nodiscard]] const sRef<DH::Texture2D>& GetTexture() const { return texture; }

	private:
		std::filesystem::path path;
		std::future<uRef<DH::TextureLoader>> pendingImage;
		sRef<DH::Texture2D> texture;
	};
}
