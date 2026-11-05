#include "AsyncTexture.hpp"

#include <chrono>
#include <exception>
#include <iostream>

namespace SV
{
	static std::filesystem::path& TextureDirectory()
	{
		static std::filesystem::path directory = SV_DEFAULT_DATA_DIR;
		return directory;
	}

	void SetTextureDirectory(const std::filesystem::path& directory)
	{
		TextureDirectory() = directory;
	}

	const std::filesystem::path& GetTextureDirectory()
	{
		return TextureDirectory();
	}

	AsyncTexture::AsyncTexture(const std::string& relativePath) : path(GetTextureDirectory() / relativePath)
	{
		// Always decode to RGBA8: 3 channel formats are OpenGL only
		pendingImage = std::async(std::launch::async, [file = path.string()]()
			{ return umake<DH::TextureLoader>(file, 4); });
	}

	bool AsyncTexture::Poll()
	{
		if (!pendingImage.valid() ||
			pendingImage.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return false;

		try
		{
			const uRef<DH::TextureLoader> image = pendingImage.get();
			const DH::Dimension2D& size = image->GetDimensions();
			const size_t decodedBytes = size.x * size.y * 4;
			if (decodedBytes > MAX_TEXTURE_BYTES)
			{
				std::cerr << "[SV::AsyncTexture] \"" << path.string() << "\" is " << size.x << 'x' << size.y
						  << " (" << decodedBytes / (1024 * 1024) << " MB decoded), over the " << MAX_TEXTURE_BYTES / (1024 * 1024)
						  << " MB texture budget: use a downscaled copy\n";
			}
			texture = DH::Texture2D::Create(*image);
			texture->GenerateMipMaps();
			return true;
		}
		catch (const std::exception& e)
		{
			std::cerr << "[SV::AsyncTexture] Failed to load \"" << path.string() << "\": " << e.what() << '\n';
			return false;
		}
	}
}
