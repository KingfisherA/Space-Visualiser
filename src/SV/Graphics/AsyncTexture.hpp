#pragma once
#include <filesystem>
#include <functional>
#include <future>
#include <string>
#include <vector>

#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Utilities/Memory.h"

namespace SV
{
	// Directory the planet texture paths are relative to (SV_DEFAULT_DATA_DIR unless overridden)
	void SetTextureDirectory(const std::filesystem::path& directory);
	const std::filesystem::path& GetTextureDirectory();

	// Textures should decode (as RGBA8) to no more than this, to bound memory use: a 2:1 map can be
	// up to about 1600x800. Larger textures still load, with a warning.
	constexpr size_t MAX_TEXTURE_BYTES = 5 * 1024 * 1024;

	// A texture whose image is decoded on a background thread. Call Poll once a frame from the
	// render thread: when decoding has finished it uploads the image (with mip maps) and returns true.
	class AsyncTexture
	{
	public:
		// Changes a decoded RGBA8 image in place (on the decoding thread) before it is uploaded
		using ImageProcessor = std::function<void(DH::TextureLoader& image)>;

		// relativePath is relative to the texture directory
		explicit AsyncTexture(const std::string& relativePath, ImageProcessor process = nullptr);

		// A texture made in memory (such as a generated ring profile), uploaded on the first Poll.
		// `data` is tightly packed texels of `format`.
		AsyncTexture(const DH::Dimension2D& dimensions, std::vector<unsigned char> data, DH::Format format);

		// Returns true on the call that makes the texture available
		bool Poll();

		[[nodiscard]] bool IsReady() const { return texture != nullptr; }
		// nullptr until the texture is ready
		[[nodiscard]] const sRef<DH::Texture2D>& GetTexture() const { return texture; }

	private:
		std::filesystem::path path;
		std::future<uRef<DH::TextureLoader>> pendingImage;
		sRef<DH::Texture2D> texture;

		// Generated textures
		DH::Dimension2D generatedDimensions{};
		std::vector<unsigned char> generatedData;
		DH::Format generatedFormat = DH::Format::None;
	};
}
