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
