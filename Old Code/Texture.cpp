#include"SV-Engine/Precompiled Global.h"
#include"Texture.h"
#include"SV-Engine/Utilities/GlobalItems.h"
#include"SV-Engine/Utilities/SVUtilities.h"
#include"SV-Engine/Utilities/Errors.h"

#include<algorithm>
#include<future>
#include<mutex>

namespace SV
{
	//
#pragma region Texture Data Class
	//
	void TextureData::AsyncLoad()
	{
		asyncThread = umake<std::thread>(&AsyncRead, this, asyncFilePath, asyncForceChannels);
	}
	//
	void TextureData::ReadTexFileData(TextureData* data, const StringView& filePath, LoadColourChannels forceChannels)
	{
		int colourChannels = -1;

		data->pixels = SOIL_load_image(filePath.GetStringArray(), &data->width, &data->height, (int*)&colourChannels, (int)forceChannels);

		const char* ptr = SOIL_last_result();

		colourChannels = (forceChannels == LoadColourChannels::Automatic) ? colourChannels : (int)forceChannels;
		switch (colourChannels)
		{
		case SOIL_LOAD_RGB:
			data->internalFormat = TexFormat::RGB;
			break;
		case SOIL_LOAD_RGBA:
			data->internalFormat = TexFormat::RGBA;
			break;
		default:
			data->internalFormat = TexFormat::RGB;
			break;
		}

		if (data->pixels == nullptr)
		{
			SV_FATAL_T(SV::TextureData::LoadTexData, ("Unable to load texture data.\n Log: " + String(SOIL_last_result())).GetStringArray());
			throw std::runtime_error(("Unable to load texture data.\n Log: " + StringView(SOIL_last_result())).GetStringArray());
		}

		data->pixelSize = GetPixelSize(data->internalFormat);
	}
	//
	void TextureData::AsyncRead(TextureData* data, const String filePath, LoadColourChannels forceChannels)
	{
		std::lock_guard<std::mutex> lg(data->asyncMutex);

		ReadTexFileData(data, filePath, forceChannels);
	}
	//
#pragma endregion
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
		this->Bind(0);
		glTexParameteri(GLenum(textureType), parameter, value);
	}
	void ITexture::SetParameter(GLenum parameter, GLfloat value)
	{
		this->Bind(0);
		glTexParameterf(GLenum(textureType), parameter, value);
	}
	void ITexture::SetParameter(GLenum parameter, const Array<GLfloat>& value)
	{
		this->Bind(0);
		glTexParameterfv(GLenum(textureType), parameter, value.GetInternalArray());
	}
	void ITexture::SetParameter(GLenum parameter, const Array<GLint>& value)
	{
		this->Bind(0);
		glTexParameteriv(GLenum(textureType), parameter, value.GetInternalArray());
	}
	//
	void ITexture::SetBorderColour(const glm::vec4& colour)
	{
		this->Bind(0);
		glTexParameterfv(GLenum(textureType), GL_TEXTURE_BORDER_COLOR, &colour[0]);
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
		this->Bind(0);
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_S, GLenum(wrapping));
		glTexParameteri(GLenum(textureType), GL_TEXTURE_WRAP_T, GLenum(wrapping));
	}
	void ITexture::SetTextureWrapping(TextureWrap sWrap, TextureWrap tWrap)
	{
		this->Bind(0);
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
		this->Bind(0);
		glGetTextureParameteriv(textureID, GL_TEXTURE_WRAP_T, &value);
		return TextureWrap(value);
	}
	//
	void ITexture::SetMinMagFilter(MinFilter minFilter, MagFilter magFilter)
	{
		this->Bind(0);

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
		this->Bind(0);

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

		this->Bind(0);
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
	const StringView TEXTURE_FILE_PATH = "../../../../Data Files/Textures/";
	SV::Texture2D* SV::Texture2D::blackTexture = nullptr;
	SV::Texture2D* SV::Texture2D::whiteTexture = nullptr;
	//
#pragma region Texture2D
//
	Texture2D::Texture2D(Ref<TextureData>& dataToLoad) : ITexture(TextureType::Texture2D, dataToLoad->width, dataToLoad->height, dataToLoad->internalFormat)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		if (dataToLoad->loadMethod == TextureData::LoadMethod::Normal)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GLenum(internalFormat), textureDimensions.width, textureDimensions.height,
				0, (GLenum)GetBaseFormat(TexFormat(internalFormat)), GL_UNSIGNED_BYTE, (const GLvoid*)dataToLoad->pixels);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else if (dataToLoad->loadMethod == TextureData::LoadMethod::Async)
		{
			this->asyncTexData = dataToLoad;

			dataToLoad->AsyncLoad();
		}
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
		static SV::Texture2D whiteTex(make <SV::TextureData>(data, 1, 1, TexFormat::RGB));
		SV::Texture2D::whiteTexture = &whiteTex;

		data.SetRange(0, { 0, 0, 0 });
		static SV::Texture2D blackTex(make <SV::TextureData>(data, 1, 1, TexFormat::RGB));
		SV::Texture2D::blackTexture = &blackTex;
	}
	//
	void Texture2D::Bind(TextureUnits textureUnit) const
	{
		ITexture::Bind(textureUnit);

		if (asyncTexData != nullptr)
		{
			if (asyncTexData->loadMethod == TextureData::LoadMethod::Async)
			{
				std::mutex& asyncMutex = asyncTexData->asyncMutex;

				if (!asyncMutex.try_lock()) return;

				if (asyncTexData->pixels == nullptr)
				{
					asyncMutex.unlock();
					return;
				}

				textureDimensions.width = asyncTexData->width;
				textureDimensions.height = asyncTexData->height;
				internalFormat = asyncTexData->internalFormat;

				glTexImage2D(GL_TEXTURE_2D, 0, GLenum(internalFormat), textureDimensions.width, textureDimensions.height,
					0, (GLenum)GetBaseFormat(TexFormat(internalFormat)), GL_UNSIGNED_BYTE, (const GLvoid*)asyncTexData->pixels);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

				asyncMutex.unlock();

				asyncTexData.reset();
			}
		}
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