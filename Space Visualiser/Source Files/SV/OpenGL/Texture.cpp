#include"SV/Precompiled Global.hpp"
#include"Texture.hpp"
#include"SV/Utilities/GlobalItems.hpp"
#include"SV/Utilities/SVUtilities.hpp"
#include"SV/Utilities/Errors.hpp"

#include<algorithm>
#include<future>
#include<mutex>
#include<numeric>

namespace SV
{
	//
#pragma region ITexture Class
	//
	ITexture::ITexture(TextureType texType, unsigned int widthIn, unsigned int heightIn, TexFormat internalFormatIn) : textureDimensions(widthIn, heightIn)
	{
		glGenTextures(1, &textureID);
		this->textureType = texType;
		this->internalFormat = internalFormatIn;
	}
	//
	ITexture::ITexture(GLuint textureIDInput, unsigned int widthIn, unsigned int heightIn, TexFormat internalFormatIn, bool param) : textureDimensions(widthIn, heightIn)
	{
		textureID = textureIDInput;
		internalFormat = internalFormatIn;
	}
	//
	ITexture::ITexture(ITexture&& tex) noexcept : textureDimensions(tex.textureDimensions)
	{
		this->textureID = tex.textureID;
		this->currentTextureUnit = tex.currentTextureUnit;
		this->textureType = tex.textureType;
		this->internalFormat = tex.internalFormat;

		tex.textureID = 0;
		tex.currentTextureUnit = 0;
		tex.textureDimensions.width = 0;
		tex.textureDimensions.height = 0;
		tex.textureType = TextureType(0);
		tex.internalFormat = TexFormat::RGB;
	}
	//
	ITexture::~ITexture()
	{
		glDeleteTextures(1, &textureID);
	}
	//
	ITexture& ITexture::operator=(ITexture&& tex) noexcept
	{
		if (this != &tex)
		{
			this->textureID = tex.textureID;
			this->currentTextureUnit = tex.currentTextureUnit;
			this->textureType = tex.textureType;
			this->internalFormat = tex.internalFormat;

			tex.textureID = 0;
			tex.currentTextureUnit = 0;
			tex.textureDimensions.width = 0;
			tex.textureDimensions.height = 0;
			tex.textureType = TextureType(0);
			tex.internalFormat = TexFormat::RGB;
		}
		return *this;
	}
	//
	void ITexture::SetParameter(GLenum parameter, GLint value)
	{
		ITexture::Bind(0);
		glTexParameteri(GLenum(textureType), parameter, value);
	}
	void ITexture::SetParameter(GLenum parameter, GLfloat value)
	{
		ITexture::Bind(0);
		glTexParameterf(GLenum(textureType), parameter, value);
	}
	void ITexture::SetParameter(GLenum parameter, const ArrayView<GLfloat> value)
	{
		ITexture::Bind(0);
		glTexParameterfv(GLenum(textureType), parameter, value.GetInternalArray());
	}
	void ITexture::SetParameter(GLenum parameter, const ArrayView<GLint> value)
	{
		ITexture::Bind(0);
		glTexParameteriv(GLenum(textureType), parameter, value.GetInternalArray());
	}
	void ITexture::SetParameter(GLenum parameter, const std::initializer_list<GLfloat>& value)
	{
		ITexture::Bind(0);
		glTexParameterfv(GLenum(textureType), parameter, value.begin());
	}
	void ITexture::SetParameter(GLenum parameter, const std::initializer_list<GLint>& value)
	{
		ITexture::Bind(0);
		glTexParameteriv(GLenum(textureType), parameter, value.begin());
	}
	//
	void ITexture::SetBorderColour(const glm::vec4& colour)
	{
		ITexture::Bind(0);
		glTexParameterfv(GLenum(textureType), GL_TEXTURE_BORDER_COLOR, &colour[0]);
	}
	glm::vec4 ITexture::GetBorderColour() const
	{
		glm::vec4 col;
		glGetTextureParameterfv(textureID, GL_TEXTURE_BORDER_COLOR, &col[0]);
		return col;
	 }
	//
	int ITexture::QueryParameterI(GLenum param) const
	{
		int val;
		glGetTextureParameteriv(textureID, param, &val);
		return val;
	}
	//
	unsigned int ITexture::QueryParameterUI(GLenum param) const
	{
		unsigned int val;
		glGetTextureParameterIuiv(textureID, param, &val);
		return val;
	}
	//
	float ITexture::QueryParameterF(GLenum param) const
	{
		float val;
		glGetTextureParameterfv(textureID, param, &val);
		return val;
	}
	//
	void ITexture::SetTextureWrapping(TextureWrap wrapping)
	{
		ITexture::Bind(0);
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_S, GLenum(wrapping));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_T, GLenum(wrapping));
	}
	void ITexture::SetTextureWrapping(TextureWrap sWrap, TextureWrap tWrap)
	{
		ITexture::Bind(0);
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_S, GLenum(sWrap));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_T, GLenum(tWrap));
	}
	//
	TextureWrap ITexture::GetSTexWrap() const
	{
		int value;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture((GLenum)textureType, textureID);
		glGetTextureParameteriv(textureID, GL_TEXTURE_WRAP_S, &value);
		return TextureWrap(value);
	}
	//
	TextureWrap ITexture::GetTTexWrap() const
	{
		int value;
		ITexture::Bind(0);
		glGetTextureParameteriv(textureID, GL_TEXTURE_WRAP_T, &value);
		return TextureWrap(value);
	}
	//
	void ITexture::SetMinMagFilter(MinFilter minFilter, MagFilter magFilter)
	{
		ITexture::Bind(0);

#ifdef SV_TEXTURE_DEBUG
		switch (minFilter)
		{
		case SV::MinFilter::Nearest:
		case SV::MinFilter::Linear:
			if (hasMipmapping) SV_WARN_T(ITexture::SetMinMagFilter, ("Note that texture with ID = " + SV::NumberToString(textureID) + " has mipmaps generated, but the use of an incompatible min filter will not use the mipmaps.").GetStringArray());
			break;
		}
#endif // SV_TEXTURE_DEBUG

		glTexParameteri(GLenum(textureType), GL_TEXTURE_MIN_FILTER, GLenum(minFilter));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_MAG_FILTER, GLenum(magFilter));
	}
	//
	MinFilter ITexture::GetMinFilter() const
	{
		int value;
		glGetTextureParameteriv(textureID, GL_TEXTURE_MIN_FILTER, &value);
		return MinFilter(value);
	}
	//
	MagFilter ITexture::GetMagFilter() const
	{
		int value;
		glGetTextureParameteriv(textureID, GL_TEXTURE_MAG_FILTER, &value);
		return MagFilter(value);
	}
	//
	void ITexture::AddMipMapping(MinFilter filterType)
	{
		ITexture::Bind(0);

#ifdef SV_TEXTURE_DEBUG
		switch (filterType)
		{
		case SV::MinFilter::Nearest:
		case SV::MinFilter::Linear:
			SV_WARN_T(ITexture::AddMipMapping, ("Mip maps cannot be used with an incompatible min filter.  (ID = " + SV::NumberToString(textureID) + ")").GetStringArray());
			break;
		}
#endif // SV_TEXTURE_DEBUG

		hasMipmapping = true;

		glGenerateMipmap(GLenum(textureType));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_MIN_FILTER, GLenum(filterType));
	}
	//
	void ITexture::SetMipMapLOD(const float minLOD, const float maxLOD, const float maxLevel)
	{
		SV_ASSERT_T(hasMipmapping, ITexture::SetMipMapLOD, ("LOD parameters cannot be set as the texture does not have any mip maps. (ID = " + SV::NumberToString(textureID) + ")").GetStringArray());

		ITexture::Bind(0);
		glTexParameterf(GLenum(textureType), GL_TEXTURE_MIN_LOD, minLOD);
		glTexParameterf(GLenum(textureType), GL_TEXTURE_MAX_LOD, maxLOD);
		glTexParameterf(GLenum(textureType), GL_TEXTURE_MAX_LEVEL, maxLevel);
	}
	//
	float ITexture::GetMinLOD() const
	{
		float value;
		glGetTextureParameterfv(textureID, GL_TEXTURE_MIN_LOD, &value);
		return value;
	}
	float ITexture::GetMaxLOD() const
	{
		float value;
		glGetTextureParameterfv(textureID, GL_TEXTURE_MAX_LOD, &value);
		return value;
	}
	float ITexture::GetMaxMipmapLevel() const
	{
		float value;
		glGetTextureParameterfv(textureID, GL_TEXTURE_MAX_LEVEL, &value);
		return value;
	}
	//
#pragma endregion
	//
#pragma region Texture Data Class
	//
	void TextureData::ReadTexFileData(const StringView& filePath, LoadColourChannels forceChannels)
	{
		int colourChannels = -1;

		pixels = SOIL_load_image(filePath.GetStringArray(), (int*)&width, (int*)&height, (int*)&colourChannels, (int)forceChannels);

		colourChannels = (forceChannels == LoadColourChannels::Automatic) ? colourChannels : (int)forceChannels;
		switch (colourChannels)
		{
		case SOIL_LOAD_RGB:
			internalFormat = TexFormat::RGB;
			break;
		case SOIL_LOAD_RGBA:
			internalFormat = TexFormat::RGBA;
			break;
		default:
			internalFormat = TexFormat::RGB;
			break;
		}

		if (pixels == nullptr)
		{
			SV_FATAL_T(SV::TextureData::LoadTexData, ("Unable to load texture data.\n Log: " + String(SOIL_last_result())).GetStringArray());
			throw std::exception(("Unable to load texture data.\n Log: " + StringView(SOIL_last_result())).GetStringArray());
		}

		pixelSize = GetPixelSize(internalFormat);
	}
	//
#pragma endregion
	//
#pragma region Async Texture Data Class
	//
	AsyncTextureData::AsyncTextureData(const SV::StringView filePath, LoadColourChannels forceChannels)
	{
		this->asyncFilePath = filePath;
		this->asyncFuture = std::async(std::launch::async, &AsyncTextureData::AsyncLoad, this, forceChannels);
	}
	//
	void AsyncTextureData::AsyncLoad(LoadColourChannels forceChannels)
	{
		int colourChannels, widthIn, heightIn;

		byte* texPixels = SOIL_load_image(asyncFilePath.GetStringArray(), &widthIn, &heightIn, (int*)&colourChannels, (int)forceChannels);

		colourChannels = (forceChannels == LoadColourChannels::Automatic) ? colourChannels : (int)forceChannels;

		if (texPixels == nullptr)
		{
			SV_FATAL_T(SV::TextureData::LoadTexData, ("Unable to load texture data.\n Log: " + String(SOIL_last_result())).GetStringArray());
			throw std::exception(("Unable to load texture data.\n Log: " + StringView(SOIL_last_result())).GetStringArray());
		}

		this->pixels = texPixels;
		this->width = widthIn;
		this->height = heightIn;
		switch (colourChannels)
		{
		case SOIL_LOAD_RGB:
			this->internalFormat = TexFormat::RGB;
			break;
		case SOIL_LOAD_RGBA:
			this->internalFormat = TexFormat::RGBA;
			break;
		default:
			this->internalFormat = TexFormat::RGB;
			break;
		}
		this->pixelSize = GetPixelSize(internalFormat);

		this->asyncFilePath.Clear();
		this->aLoaded = true;
	}
	//
#pragma endregion
	//
#pragma region Async Texture2D
	//
	AsyncTexture2D::AsyncTexture2D(const Ref<AsyncTextureData>& dataIn) :
		ITexture(SV::TextureType::Texture2D, 0, 0, SV::TexFormat::RGB),
		texData(dataIn),
		iParams(),
		fParams()
	{
		if (dataIn == nullptr) throw std::exception("AsyncTextureData cannot be null pointer when constructing AsyncTexture2D");
		TryLoadData();
	}
	AsyncTexture2D::AsyncTexture2D(AsyncTexture2D&& texture) noexcept :
		ITexture(std::move(texture)),
		texData(std::move(texture.texData)),
		iParams(std::move(texture.iParams)),
		fParams(std::move(texture.fParams)),
		texLoaded(texture.texLoaded),
		borderCol(texture.borderCol)
	{
		TryLoadData();
	}
	//
	AsyncTexture2D& AsyncTexture2D::operator=(AsyncTexture2D&& tex) noexcept
	{
		if (this != &tex)
		{
			this->textureID = tex.textureID;
			this->currentTextureUnit = tex.currentTextureUnit;
			this->textureType = tex.textureType;
			this->internalFormat = tex.internalFormat;

			this->texData = std::move(texData);
			this->iParams = std::move(tex.iParams);
			this->fParams = std::move(tex.fParams);
			this->texLoaded = tex.texLoaded;
			this->borderCol = tex.borderCol;

			tex.textureID = 0;
			tex.currentTextureUnit = 0;
			tex.textureDimensions.width = 0;
			tex.textureDimensions.height = 0;
			tex.textureType = TextureType(0);
			tex.internalFormat = TexFormat::RGB;
		}
		return *this;
	}
	//
	bool AsyncTexture2D::TryLoadData()
	{
		if (!texLoaded && texData->Loaded() && texData->GetPixelArray() != nullptr)
		{
			this->internalFormat = texData->GetInternalFormat();
			this->textureDimensions.width = texData->GetWidth();
			this->textureDimensions.height = texData->GetHeight();

			glBindTexture(GL_TEXTURE_2D, textureID);
			glTexImage2D(GL_TEXTURE_2D, 0, GLenum(internalFormat), textureDimensions.width, textureDimensions.height,
				0, (GLenum)GetBaseFormat(TexFormat(internalFormat)), GL_UNSIGNED_BYTE, (const GLvoid*)texData->GetPixelArray());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &borderCol[0]);

			if (hasMipmapping) glGenerateMipmap(GL_TEXTURE_2D);

			for (auto& [parameter, value] : iParams)
			{
				glTexParameteri(GL_TEXTURE_2D, parameter, value);
			}
			iParams.clear();

			for (auto& [parameter, value] : fParams)
			{
				glTexParameterf(GL_TEXTURE_2D, parameter, value);
			}
			fParams.clear();

			glBindTexture(GL_TEXTURE_2D, 0);

			texData.reset();
			borderCol = glm::vec4{ 0.0f };

			texLoaded = true;
		}
		return texLoaded;
	}
	void AsyncTexture2D::SetParameter(GLenum parameter, GLint value)
	{
		if (texLoaded)
		{
			ITexture::SetParameter(parameter, value);
			return;
		}

		iParams[parameter] = value;
	}
	void AsyncTexture2D::SetParameter(GLenum parameter, GLfloat value)
	{
		if (texLoaded)
		{
			ITexture::SetParameter(parameter, value);
			return;
		}

		fParams[parameter] = value;
	}
	//
	void AsyncTexture2D::SetBorderColour(const glm::vec4& colour)
	{
		if (texLoaded)
		{
			ITexture::SetBorderColour(colour);
			return;
		}

		borderCol = colour;
	}
	glm::vec4 AsyncTexture2D::GetBorderColour() const
	{
		if (texLoaded) return ITexture::GetBorderColour();

		return this->borderCol;
	 }
	//
	int AsyncTexture2D::QueryParameterI(GLenum param) const
	{
		if (texLoaded)
		{
			return ITexture::QueryParameterI(param);
		}

		auto kvPair = iParams.find(param);
		if (kvPair == iParams.end()) return -1;
		return kvPair->second;
	}
	unsigned int AsyncTexture2D::QueryParameterUI(GLenum param) const
	{
		if (texLoaded)
		{
			return ITexture::QueryParameterUI(param);
		}

		auto kvPair = iParams.find(param);
		if (kvPair == iParams.end()) return IndexUndefined;
		return kvPair->second;
	}
	float AsyncTexture2D::QueryParameterF(GLenum param) const
	{
		if (texLoaded)
		{
			return ITexture::QueryParameterF(param);
		}

		auto kvPair = fParams.find(param);
		if (kvPair == fParams.end()) return IndexUndefined;
		return kvPair->second;
	}
	//
	void AsyncTexture2D::SetTextureWrapping(TextureWrap wrapping)
	{
		AsyncTexture2D::SetTextureWrapping(wrapping, wrapping);
	}
	void AsyncTexture2D::SetTextureWrapping(TextureWrap sWrap, TextureWrap tWrap)
	{
		if (texLoaded)
		{
			ITexture::SetTextureWrapping(sWrap, tWrap);
			return;
		}

		iParams[GL_TEXTURE_WRAP_S] = (int)sWrap;
		iParams[GL_TEXTURE_WRAP_T] = (int)tWrap;
	}
	//
	void AsyncTexture2D::AddMipMapping(MinFilter filterType)
	{
		if (texLoaded) return ITexture::AddMipMapping(filterType);

#ifdef SV_TEXTURE_DEBUG
		switch (filterType)
		{
		case SV::MinFilter::Nearest:
		case SV::MinFilter::Linear:
			SV_WARN_T(ITexture::AddMipMapping, ("Mip maps cannot be used with an incompatible min filter.  (ID = " + SV::NumberToString(textureID) + ")").GetStringArray());
			break;
		}
#endif // SV_TEXTURE_DEBUG

		hasMipmapping = true;

		iParams[GL_TEXTURE_MIN_FILTER] = (int)filterType;
	}
	void AsyncTexture2D::SetMipMapLOD(const float minLOD, const float maxLOD, const float maxLevel)
	{
		if (texLoaded) return ITexture::SetMipMapLOD(minLOD, maxLOD, maxLevel);
		SV_ASSERT_T(hasMipmapping, AsyncTexture2D::SetMipMapLOD, ("LOD parameters cannot be set as the texture does not have any mip maps. (ID = " + SV::NumberToString(textureID) + ")").GetStringArray());

		fParams[GL_TEXTURE_MIN_LOD] = minLOD;
		fParams[GL_TEXTURE_MAX_LOD] = maxLOD;
		fParams[GL_TEXTURE_MAX_LEVEL] = maxLevel;
	}
	float AsyncTexture2D::GetMinLOD() const
	{
		if (texLoaded) return ITexture::GetMinLOD();

		const auto kvPair = fParams.find((TexParam)GL_TEXTURE_MIN_LOD);
		if (kvPair == fParams.end()) return std::numeric_limits<float>::quiet_NaN();
		return kvPair->second;
	}
	float AsyncTexture2D::GetMaxLOD() const
	{
		if (texLoaded) return ITexture::GetMaxLOD();

		const auto kvPair = fParams.find((TexParam)GL_TEXTURE_MAX_LOD);
		if (kvPair == fParams.end()) return std::numeric_limits<float>::quiet_NaN();
		return kvPair->second;
	}
	float AsyncTexture2D::GetMaxMipmapLevel() const
	{
		if (texLoaded) return ITexture::GetMaxMipmapLevel();

		const auto kvPair = fParams.find((TexParam)GL_TEXTURE_MAX_LEVEL);
		if (kvPair == fParams.end()) return std::numeric_limits<float>::quiet_NaN();
		return kvPair->second;
	}
	//
#pragma endregion
	//
	const StringView TEXTURE_FILE_PATH = "..\\..\\..\\..\\Data Files\\Textures\\";
	SV::Texture2D* SV::Texture2D::blackTexture = nullptr;
	SV::Texture2D* SV::Texture2D::whiteTexture = nullptr;
	//
#pragma region Texture2D
//
	Texture2D::Texture2D(const TextureData& dataToLoad) : ITexture(TextureType::Texture2D, dataToLoad.width, dataToLoad.height, dataToLoad.internalFormat)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GLenum(internalFormat), textureDimensions.width, textureDimensions.height,
			0, (GLenum)GetBaseFormat(TexFormat(internalFormat)), GL_UNSIGNED_BYTE, (const GLvoid*)dataToLoad.pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	//
	Texture2D::Texture2D(unsigned int widthA, unsigned int heightA, TexFormat textureFormat) :ITexture(TextureType::Texture2D, widthA, heightA, textureFormat)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GLenum(textureFormat), textureDimensions.width, textureDimensions.height, 0, (GLenum)GetBaseFormat(TexFormat(internalFormat)), GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	//
	Texture2D::Texture2D(unsigned int widthA, unsigned int heightA, TexFormat textureFormat, unsigned int samples) : ITexture(TextureType::Texture2DMultisampled, widthA, heightA, textureFormat)
	{
		SV_ASSERT_T(samples == 0, SV::Texture2D::Texture2D, "The texture must have more than 0 samples if the sampling parameter is being used.");

#ifdef SV_DEBUG
		if (!glIsEnabled(GL_MULTISAMPLE)) SV_WARN_T(SV::RenderBuffer::RenderBuffer, "Note that GL_MULTISAMPLE is not enabled. To use multisapled render buffers, GL_MULTISAMPLE must be enabled.");
#endif // SV_DEBUG


		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GLenum(textureFormat), widthA, heightA, GL_TRUE);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
	}
	//
	Texture2D::Texture2D(Texture2D&& texture) noexcept : ITexture((ITexture&&)texture) {}
	//
	void Texture2D::InitiliseStaticTextures()
	{
		Array<byte> data = { 255, 255, 255 };
		static SV::Texture2D whiteTex(SV::TextureData(data, 1, 1, TexFormat::RGB));
		SV::Texture2D::whiteTexture = &whiteTex;

		data.SetRange(0, { 0, 0, 0 });
		static SV::Texture2D blackTex(SV::TextureData(data, 1, 1, TexFormat::RGB));
		SV::Texture2D::blackTexture = &blackTex;
	}
	//
#pragma endregion
	//
#pragma region Texture3D
	//
	Texture3D::Texture3D(unsigned int widthA, unsigned int heightA, unsigned int depthA, TexFormat textureFormat) : ITexture(TextureType::Texture3D, widthA, heightA, textureFormat)
	{
		depth = depthA;

		glBindTexture(GL_TEXTURE_3D, textureID);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexImage3D(GL_TEXTURE_3D, 0, GLenum(textureFormat), textureDimensions.width, textureDimensions.height, depth, 0,
			(GLenum)GetBaseFormat(TexFormat(internalFormat)), GLenum(GetTexPrimitiveType(textureFormat)), nullptr);
	}
	//
	Texture3D::Texture3D(Texture3D&& tex3D) noexcept : ITexture((ITexture&&)tex3D)
	{
		depth = tex3D.depth;
	}
	//
	Texture3D& Texture3D::operator=(Texture3D&& tex) noexcept
	{
		if (this != &tex)
		{
			this->textureID = tex.textureID;
			this->currentTextureUnit = tex.currentTextureUnit;
			this->textureType = tex.textureType;
			this->internalFormat = tex.internalFormat;
			this->depth = tex.depth;

			tex.textureID = 0;
			tex.currentTextureUnit = 0;
			tex.textureDimensions.width = 0;
			tex.textureDimensions.height = 0;
			tex.depth = 0;
			tex.textureType = TextureType(0);
			tex.internalFormat = TexFormat::RGB;
		}
		return *this;
	}
	//
	void Texture3D::SetTextureWrapping(TextureWrap wrapping)
	{
		this->Bind(0);
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_S, GLenum(wrapping));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_T, GLenum(wrapping));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_R, GLenum(wrapping));
	}
	void Texture3D::SetTextureWrapping(TextureWrap sWrap, TextureWrap tWrap, TextureWrap rWrap)
	{
		this->Bind(0);
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_S, GLenum(sWrap));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_T, GLenum(tWrap));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_R, GLenum(rWrap));
	}
	//
	TextureWrap Texture3D::GetRTexWrap() const
	{
		int value;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture((GLenum)textureType, textureID);
		glGetTextureParameteriv(textureID, GL_TEXTURE_WRAP_R, &value);
		return TextureWrap(value);
	}
	//
#pragma endregion
//
#pragma region Depth Texture
	//
	DepthTexture2D::DepthTexture2D(unsigned int width, unsigned int height, DepthFormat format) : ITexture(TextureType::Texture2D, width, height, TexFormat::Depth)
	{
		depthFormat = format;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GLenum(format), width, height,
			0, GL_DEPTH_COMPONENT, GL_FLOAT, (GLvoid*)nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	//
	DepthTexture2D::DepthTexture2D(DepthTexture2D&& dTex) noexcept : ITexture((ITexture&&)dTex)
	{
		this->depthFormat = dTex.depthFormat;
	}
	//
	DepthTexture2D& DepthTexture2D::operator=(DepthTexture2D&& tex) noexcept
	{
		if (this != &tex)
		{
			this->textureID = tex.textureID;
			this->currentTextureUnit = tex.currentTextureUnit;
			this->textureType = tex.textureType;
			this->internalFormat = tex.internalFormat;
			this->depthFormat = tex.depthFormat;

			tex.textureID = 0;
			tex.currentTextureUnit = 0;
			tex.textureDimensions.width = 0;
			tex.textureDimensions.height = 0;
			tex.textureType = TextureType(0);
			tex.internalFormat = TexFormat::RGB;
		}
		return *this;
	}
	//
#pragma endregion

	//
}