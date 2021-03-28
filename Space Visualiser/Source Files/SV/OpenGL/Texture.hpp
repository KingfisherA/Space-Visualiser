#pragma once
#include<thread>
#include<mutex>
#include<future>
#include<initializer_list>

#include"SV/Utilities/GlobalItems.hpp"
#include"OpenGL Context.hpp"
#include"SV/Utilities/Array.hpp"
#include"SV/Utilities/Array.hpp"
#include"SV/Utilities/String.hpp"
#include"SV/Utilities/Errors.hpp"


using TextureUnits = unsigned int;
constexpr unsigned int TEX_UNIT_UNDEFINED = 0xffffffff;

#ifdef SV_DEBUG
#define SV_TEXTURE_DEBUG
#endif // SV_DEBUG

namespace SV
{
	using TexParam = unsigned int;
	extern const StringView TEXTURE_FILE_PATH;

#pragma region Dimension Structures
	//
	struct Dimension2D
	{
	public:
		constexpr Dimension2D() :width(0), height(0) {}
		constexpr Dimension2D(unsigned int w) : width(w), height(0) {}
		constexpr Dimension2D(unsigned int w, unsigned int h) : width(w), height(h) {}
		constexpr Dimension2D(const Dimension2D& d) : width(d.width), height(d.height) {}
		//
		void operator=(const Dimension2D& d)
		{
			width = d.width;
			height = d.height;
		}
		//
		bool operator==(const Dimension2D& d)const
		{
			return width == d.width && height == d.height;
		}
		bool operator!=(const Dimension2D& d)const
		{
			return width != d.width || height != d.height;
		}
		bool operator<(const Dimension2D& d)const
		{
			return (width * height) < (d.width * d.height);
		}
		bool operator<=(const Dimension2D& d)const
		{
			return (width * height) <= (d.width * d.height);
		}
		bool operator>(const Dimension2D& d)const
		{
			return (width * height) > (d.width * d.height);
		}
		bool operator>=(const Dimension2D& d)const
		{
			return (width * height) >= (d.width * d.height);
		}
		//
		unsigned int width;
		unsigned int height;
	};
	//
	struct Dimension3D
	{
	public:
		constexpr Dimension3D() :width(0), height(0), depth(0) {}
		constexpr Dimension3D(unsigned int w) : width(w), height(0), depth(0) {}
		constexpr Dimension3D(unsigned int w, unsigned int h) : width(w), height(h), depth(0) {}
		constexpr Dimension3D(unsigned int w, unsigned int h, unsigned int d) : width(w), height(h), depth(d) {}
		constexpr Dimension3D(const Dimension3D& d) : width(d.width), height(d.height), depth(d.depth) {}
		//
		void operator=(const Dimension2D& d)
		{
			width = d.width;
			height = d.height;
		}
		//
		bool operator==(const Dimension2D& d)const
		{
			return width == d.width && height == d.height;
		}
		bool operator!=(const Dimension2D& d)
		{
			return width != d.width || height != d.height;
		}
		bool operator<(const Dimension2D& d)const
		{
			return (width * height) < (d.width * d.height);
		}
		bool operator<=(const Dimension2D& d)const
		{
			return (width * height) <= (d.width * d.height);
		}
		bool operator>(const Dimension2D& d)const
		{
			return (width * height) > (d.width * d.height);
		}
		bool operator>=(const Dimension2D& d)const
		{
			return (width * height) >= (d.width * d.height);
		}
		//
		void operator=(const Dimension3D& d)
		{
			width = d.width;
			height = d.height;
			depth = d.depth;
		}
		//
		bool operator==(const Dimension3D& d)const
		{
			return width == d.width && height == d.height && depth == d.depth;
		}
		bool operator!=(const Dimension3D& d)const
		{
			return width != d.width || height != d.height || depth != d.depth;
		}
		bool operator<(const Dimension3D& d)const
		{
			return (width * height * depth) < (d.width * d.height * d.depth);
		}
		bool operator<=(const Dimension3D& d)const
		{
			return (width * height * depth) <= (d.width * d.height * d.depth);
		}
		bool operator>(const Dimension3D& d)const
		{
			return (width * height * depth) > (d.width * d.height * d.depth);
		}
		bool operator>=(const Dimension3D& d)const
		{
			return (width * height * depth) >= (d.width * d.height * d.depth);
		}
		//
		unsigned int width;
		unsigned int height;
		unsigned int depth;
	};
	//
#pragma endregion

#pragma region Texture Enumerations
	//
	enum class LoadColourChannels
	{
		Automatic = SOIL_LOAD_AUTO,
		RGB = SOIL_LOAD_RGB,
		RGBA = SOIL_LOAD_RGBA
	};
	//
	enum class TexFormat
	{
		//Identifies this texture as a depth texture
		//Use "DepthFormat" to specify the internal format of a depth texture
		Depth = GL_DEPTH_COMPONENT,
		R = GL_RED,
		RG = GL_RG,
		RGB = GL_RGB,
		RGBA = GL_RGBA,
		//
		R8 = GL_R8,
		R8Normalised = GL_R8_SNORM,
		//Integer
		RGB16I = GL_RGB16I,
		RGB32I = GL_RGB32I,
		RGBA16I = GL_RGBA16I,
		RGBA32I = GL_RGBA32I,
		//Unsigned int
		RGB16UI = GL_RGB16UI,
		RGB32UI = GL_RGB32UI,
		RGBA16UI = GL_RGBA16UI,
		RGBA32UI = GL_RGBA32UI,
		//Floating point
		RGB16F = GL_RGB16F,
		RGB32F = GL_RGB32F,
		RGBA16F = GL_RGBA16F,
		RGBA32F = GL_RGBA32F
	};
	//
	enum class DepthFormat
	{
		//
		Depth16BitFloat = GL_DEPTH_COMPONENT16,
		//
		Depth24BitFloat = GL_DEPTH_COMPONENT24,
		//
		Depth32BitFloat = GL_DEPTH_COMPONENT32F
	};
	//
	enum class TextureType
	{
		Texture2D = GL_TEXTURE_2D,
		Texture3D = GL_TEXTURE_3D,
		Texture2DMultisampled = GL_TEXTURE_2D_MULTISAMPLE
	};
	//
	enum class TextureWrap
	{
		ClampToEdge = GL_CLAMP_TO_EDGE,
		ClampToBorder = GL_CLAMP_TO_BORDER,
		Repeat = GL_REPEAT,
		MirroredRepeat = GL_MIRRORED_REPEAT,
		MirrorClampToEdge = GL_MIRROR_CLAMP_TO_EDGE
	};
	//
	enum class MagFilter
	{
		Nearest = GL_NEAREST,
		Linear = GL_LINEAR
	};
	enum class MinFilter
	{
		Nearest = GL_NEAREST,
		Linear = GL_LINEAR,
		MipmapNearestNearest = GL_NEAREST_MIPMAP_NEAREST,
		MipmapNearestLinear = GL_NEAREST_MIPMAP_LINEAR,
		MipmapLinearNearest = GL_LINEAR_MIPMAP_NEAREST,
		MipmapLinearLinear = GL_LINEAR_MIPMAP_LINEAR
	};
	//
#pragma endregion

	//
	struct TextureWrapOptions
	{
	public:
		constexpr TextureWrapOptions() = default;
		constexpr TextureWrapOptions(const TextureWrapOptions&) = default;
		constexpr TextureWrapOptions(TextureWrapOptions&&) = delete;
		/// <summary>
		/// Create a texture wrap options with parameters
		/// </summary>
		/// <param name="tX">The X tiling</param>
		/// <param name="tY">The Y tiling</param>
		/// <param name="oX">The X offset</param>
		/// <param name="oY">The Y offset</param>
		constexpr TextureWrapOptions(float tX, float tY, float oX, float oY) :
			TileX(tX),
			TileY(tY),
			OffsetX(oX),
			OffsetY(oY) {}
		constexpr TextureWrapOptions(const glm::vec2& tilingIn, const glm::vec2& offsetIn) :
			tiling(tilingIn),
			offset(offsetIn) {}
		constexpr TextureWrapOptions(const glm::vec4& tilingOffsetIn) :
			tiling(tilingOffsetIn.x, tilingOffsetIn.y),
			offset(tilingOffsetIn.z, tilingOffsetIn.w) {}
		union
		{
			glm::vec2 tiling{ 0.0f };
			struct
			{
				/// <summary>
			/// The X tiling
			/// </summary>
				float TileX;
				/// <summary>
				/// The Y tiling
				/// </summary>
				float TileY;
			};
		};
		//
		union
		{
			glm::vec2 offset{ 0.0f };
			struct
			{
				/// <summary>
				/// The X offset
				/// </summary>
				float OffsetX;
				/// <summary>
				/// The Y offset
				/// </summary>
				float OffsetY;
			};
		};
	};
	constexpr TextureWrapOptions DefaultTextureOptions = { 1, 1, 0, 0 };

	//Add get border colour function

	//
	class ITexture abstract
	{
	public:
		//
		enum class TextureClass : unsigned char
		{
			Texture2D,
			AsyncTexture2D,
			Texture3D,
			DepthTexture2D
		};
		//
		virtual TextureClass GetClass() const = 0;
		//
		ITexture(const ITexture& tex) = delete;
		ITexture(ITexture&& tex) noexcept;
		virtual ~ITexture();
		//
		//Copy and move assignment operators
		ITexture& operator=(const ITexture&) = delete;
		ITexture& operator=(ITexture&& tex) noexcept;
		//
		void Bind(TextureUnits textureUnit) const
		{
			glActiveTexture(GL_TEXTURE0 + (int)textureUnit);
			glBindTexture(GLenum(textureType), textureID);
			currentTextureUnit = textureUnit;
		}
		void Unbind() const
		{
			glActiveTexture(GL_TEXTURE0 + currentTextureUnit);
			glBindTexture(GLenum(textureType), 0);
			currentTextureUnit = 0;
		}
		void Unbind(TextureUnits textureUnit)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GLenum(textureType), 0);
		}
		static void Unbind(TextureType textureType, TextureUnits textureUnit)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GLenum(textureType), 0);
		}
		//
		constexpr static TexFormat GetBaseFormat(TexFormat internalFormat)
		{
			switch (internalFormat)
			{
			case SV::TexFormat::R:
			case SV::TexFormat::RG:
			case SV::TexFormat::RGB:
			case SV::TexFormat::RGBA:
				return internalFormat;
				break;
			case SV::TexFormat::R8:
				return TexFormat::R;
				break;
			case SV::TexFormat::R8Normalised:
				return TexFormat::R;
				break;
			case SV::TexFormat::RGB16I:
				return TexFormat::RGB;
				break;
			case SV::TexFormat::RGB32I:
				return TexFormat::RGB;
				break;
			case SV::TexFormat::RGBA16I:
				return TexFormat::RGBA;
				break;
			case SV::TexFormat::RGBA32I:
				return TexFormat::RGBA;
				break;
			case SV::TexFormat::RGB16UI:
				return TexFormat::RGB;
				break;
			case SV::TexFormat::RGB32UI:
				return TexFormat::RGB;
				break;
			case SV::TexFormat::RGBA16UI:
				return TexFormat::RGBA;
				break;
			case SV::TexFormat::RGBA32UI:
				return TexFormat::RGBA;
				break;
			case SV::TexFormat::RGB16F:
				return TexFormat::RGB;
				break;
			case SV::TexFormat::RGB32F:
				return TexFormat::RGB;
				break;
			case SV::TexFormat::RGBA16F:
				return TexFormat::RGBA;
				break;
			case SV::TexFormat::RGBA32F:
				return TexFormat::RGBA;
				break;
			default:
				return TexFormat::RGB;
				break;
			}
		}
		constexpr static GLType GetTexPrimitiveType(TexFormat internalFormat)
		{
			switch (internalFormat)
			{
			case SV::TexFormat::R:
				return GLType::UByte;
				break;
			case SV::TexFormat::RG:
				return GLType::UByte;
				break;
			case SV::TexFormat::RGB:
				return GLType::UByte;
				break;
			case SV::TexFormat::RGBA:
				return GLType::UByte;
				break;
			case SV::TexFormat::R8:
				return GLType::UByte;
				break;
			case SV::TexFormat::R8Normalised:
				return GLType::UByte;
				break;
			case SV::TexFormat::RGB16I:
				return GLType::Short;
				break;
			case SV::TexFormat::RGB32I:
				return GLType::Int;
				break;
			case SV::TexFormat::RGBA16I:
				return GLType::Short;
				break;
			case SV::TexFormat::RGBA32I:
				return GLType::Int;
				break;
			case SV::TexFormat::RGB16UI:
				return GLType::UShort;
				break;
			case SV::TexFormat::RGB32UI:
				return GLType::UInt;
				break;
			case SV::TexFormat::RGBA16UI:
				return GLType::UShort;
				break;
			case SV::TexFormat::RGBA32UI:
				return GLType::UInt;
				break;
			case SV::TexFormat::RGB16F:
				return GLType::HalfFloat;
				break;
			case SV::TexFormat::RGB32F:
				return GLType::Float;
				break;
			case SV::TexFormat::RGBA16F:
				return GLType::HalfFloat;
				break;
			case SV::TexFormat::RGBA32F:
				return GLType::Float;
				break;
			default:
				return GLType::UByte;
				break;
			}
		}
		//
		virtual void SetParameter(GLenum parameter, GLint value);
		virtual void SetParameter(GLenum parameter, GLfloat value);
		void SetParameter(GLenum parameter, const SV::ArrayView<GLfloat> value);
		void SetParameter(GLenum parameter, const SV::ArrayView<GLint> value);
		void SetParameter(GLenum parameter, const std::initializer_list<GLfloat>& value);
		void SetParameter(GLenum parameter, const std::initializer_list<GLint>& value);
		//
		virtual void SetBorderColour(const glm::vec4& colour);
		virtual glm::vec4 GetBorderColour() const;
		//
		virtual int QueryParameterI(GLenum param) const;
		virtual unsigned int QueryParameterUI(GLenum param) const;
		virtual float QueryParameterF(GLenum param) const;
		//
		virtual void SetTextureWrapping(TextureWrap wrapping);
		virtual void SetTextureWrapping(TextureWrap sWrap, TextureWrap tWrap);
		TextureWrap GetSTexWrap() const;
		TextureWrap GetTTexWrap() const;
		//
		virtual void SetMinMagFilter(MinFilter minFilter, MagFilter magFilter);
		MinFilter GetMinFilter() const;
		MagFilter GetMagFilter() const;
		//
		inline GLuint GetTextureID() const { return textureID; }
		inline TextureUnits GetCurrentUnit() const { return currentTextureUnit; }
		//
		inline unsigned int GetHeight() const { return textureDimensions.height; }
		inline unsigned int GetWidth() const { return textureDimensions.width; }
		inline const Dimension2D& GetDimension() const { return textureDimensions; }
		inline TexFormat GetInternalFormat() const { return internalFormat; }
		inline TextureType GetTextureType() const { return textureType; }
		inline bool IsMultisampled() const { return GLenum(textureType) == GL_TEXTURE_2D_MULTISAMPLE; }
		//
		virtual void AddMipMapping(MinFilter filterType = MinFilter::MipmapLinearLinear);
		inline const bool GetMipMapping() const { return hasMipmapping; }
		//
		virtual 	void SetMipMapLOD(const float minLOD, const float maxLOD, const float maxLevel);
		virtual float GetMinLOD() const;
		virtual float GetMaxLOD() const;
		virtual float GetMaxMipmapLevel() const;
		//
	protected:
		//
		ITexture(TextureType texType, unsigned int widthIn, unsigned int heightIn, TexFormat internalFormatIn);
		ITexture(GLuint textureIDInput, unsigned int widthIn, unsigned int heightIn, TexFormat internalFormatIn, bool param);
		//
		TextureType textureType = TextureType::Texture2D;
		//The number of colour componets for each pixel
		mutable TexFormat internalFormat = TexFormat::RGB;
		//
		mutable TextureUnits currentTextureUnit = 0;
		//
		mutable Dimension2D textureDimensions;
		//
		bool hasMipmapping = false;
		//
		GLuint textureID = 0;
	};

	//
	class TextureData
	{
	public:
		explicit TextureData(const StringView& filepath, LoadColourChannels forceChannels = LoadColourChannels::Automatic)
		{
			ReadTexFileData(filepath, forceChannels);
		}
		explicit TextureData(const SV::Array<byte>& pixelsIn, unsigned int widthIn, unsigned int heightIn, TexFormat internalFormatIn)
		{
			unsigned int arrayLength = widthIn * heightIn * GetPixelSize(ITexture::GetBaseFormat(internalFormatIn));
			if (pixelsIn.GetCount() != arrayLength)
			{
				SV_FATAL_THROW(SV::TextureData::TextureData, std::runtime_error, "Array does not fit resolution");
			}

			pixels = (byte*)malloc(arrayLength);

			if (pixels == nullptr)
			{
				SV_FATAL_THROW(SV::TextureData::TextureData, std::runtime_error, "Cannot allocate memory for temporary array");
			}

			for (unsigned int i = 0; i < arrayLength; i++)
			{
				pixels[i] = pixelsIn[i];
			}

			internalFormat = internalFormatIn;
			pixelSize = GetPixelSize(internalFormatIn);
			width = widthIn;
			height = heightIn;
		}
		virtual ~TextureData()
		{
			free(pixels);
			pixels = nullptr;
		}
		//
		inline TexFormat GetInternalFormat() const { return internalFormat; }
		//
		inline unsigned int GetWidth() const { return width; }
		inline unsigned int GetHeight() const { return height; }
		inline unsigned int GetPixelCount() const { return width * height; }
		inline unsigned long GetByteCount() const { return width * height * unsigned long(pixelSize); }
		//Get the byte length of a pixel TexFormat
		constexpr static unsigned int GetPixelSize(TexFormat format)
		{
			switch (format)
			{
			case TexFormat::RGB:
				return 3;
			case TexFormat::RGB16UI:
			case TexFormat::RGB16I:
			case TexFormat::RGB16F:
				return 6;
			case TexFormat::RGB32UI:
			case TexFormat::RGB32I:
			case TexFormat::RGB32F:
				return 12;
			case TexFormat::RGBA:
				return 4;
			case TexFormat::RGBA16UI:
			case TexFormat::RGBA16I:
			case TexFormat::RGBA16F:
				return 8;
			case TexFormat::RGBA32UI:
			case TexFormat::RGBA32I:
			case TexFormat::RGBA32F:
				return 16;
			case TexFormat::R:
			case TexFormat::R8Normalised:
			case TexFormat::R8:
				return 1;
			case TexFormat::RG:
				return 2;
			case TexFormat::Depth:
				return 2;
			default:
				return 1;
			}
		}
		//
		inline const byte* GetPixelArray() const { return pixels; }
		//
		void AsyncLoad();
		//
	protected:
		explicit TextureData() {}
		//
		friend class Texture2D;
		friend class Texture3D;
		//
		void ReadTexFileData(const StringView& filePath, LoadColourChannels forceChannels);
		//
		byte* pixels = nullptr;
		//
		int width = 0, height = 0;
		unsigned int pixelSize = 3;
		//
		TexFormat internalFormat = TexFormat::RGB;
	};

	//
	class AsyncTextureData : public TextureData
	{
	public:
		AsyncTextureData(const SV::StringView filePath, LoadColourChannels forceChannels = LoadColourChannels::Automatic);
		//
		bool Loaded() const { return aLoaded; }
		const std::future<void>& GetFuture() const { return asyncFuture; }
		//
	private:

		void AsyncLoad(LoadColourChannels forceChannels = LoadColourChannels::Automatic);

		SV::String asyncFilePath;

		std::future<void> asyncFuture;
		std::atomic_bool aLoaded = false;
	};

	//
	class Texture2D : public ITexture
	{
	public:
		//
		Texture2D(const TextureData& dataToLoad);
		Texture2D(unsigned int widthA, unsigned int heightA, TexFormat textureFormat);
		Texture2D(unsigned int widthA, unsigned int heightA, TexFormat textureFormat, unsigned int samples);
		Texture2D(Texture2D&& texture) noexcept;
		Texture2D(const Texture2D&) = delete;
		//
		static void InitiliseStaticTextures();
		static inline Texture2D* GetWhiteTex() { return whiteTexture; }
		static inline Texture2D* GetBlackTex() { return blackTexture; }
		//
		virtual TextureClass GetClass() const override { return TextureClass::Texture2D; }
		//
	private:
		//
		static SV::Texture2D* blackTexture;
		static SV::Texture2D* whiteTexture;
	};

	//
	class AsyncTexture2D : public ITexture
	{
	public:
		AsyncTexture2D(const Ref<AsyncTextureData>& dataIn);
		AsyncTexture2D(AsyncTexture2D&& texture) noexcept;
		AsyncTexture2D(const AsyncTexture2D&) = delete;

		AsyncTexture2D& operator=(const AsyncTexture2D&) = delete;
		AsyncTexture2D& operator=(AsyncTexture2D&& tex) noexcept;
		//
		bool TryLoadData();
		bool IsLoaded() const { return texLoaded; }
		//
		virtual void SetParameter(GLenum parameter, GLint value) override;
		virtual void SetParameter(GLenum parameter, GLfloat value) override;
		//
		virtual	void SetBorderColour(const glm::vec4& colour) override;
		virtual glm::vec4 GetBorderColour() const override;
		//
		virtual int QueryParameterI(GLenum param) const override;
		virtual unsigned int QueryParameterUI(GLenum param) const override;
		virtual float QueryParameterF(GLenum param) const override;
		//
		virtual void SetTextureWrapping(TextureWrap wrapping) override;
		virtual void SetTextureWrapping(TextureWrap sWrap, TextureWrap tWrap) override;
		//
		virtual void AddMipMapping(MinFilter filterType = MinFilter::MipmapLinearLinear) override;
		//
		virtual void SetMipMapLOD(const float minLOD, const float maxLOD, const float maxLevel) override;
		virtual float GetMinLOD() const override;
		virtual float GetMaxLOD() const override;
		virtual float GetMaxMipmapLevel() const override;
		//
		virtual TextureClass GetClass() const override { return TextureClass::AsyncTexture2D; }
		//
	private:

		Ref<AsyncTextureData> texData = nullptr;
		bool texLoaded = false;
		glm::vec4 borderCol;

		std::unordered_map<TexParam, int> iParams;
		std::unordered_map<TexParam, float> fParams;
	};

	//
	class Texture3D : public ITexture
	{
	public:
		Texture3D(unsigned int widthA, unsigned int heightA, unsigned int depthA, TexFormat textureFormat);
		Texture3D(const Texture3D& tex3D) = delete;
		Texture3D(Texture3D&& tex3D) noexcept;
		//Copy and move assignment operators
		Texture3D& operator=(const Texture3D&) = delete;
		Texture3D& operator=(Texture3D&& tex) noexcept;
		//
		inline unsigned int GetDepth() { return depth; }
		inline const Dimension3D& GetDimension() { return *((const Dimension3D*)&textureDimensions); }
		//
		virtual void SetTextureWrapping(TextureWrap wrapping) override;
		void SetTextureWrapping(TextureWrap sWrap, TextureWrap tWrap, TextureWrap rWrap);
		TextureWrap GetRTexWrap() const;
		//
		virtual TextureClass GetClass() const override { return TextureClass::Texture3D; }
		//
	protected:
		unsigned int depth = 0;
	};

	//
	class DepthTexture2D : public ITexture
	{
	public:
		DepthTexture2D(unsigned int width, unsigned int height, DepthFormat format);
		DepthTexture2D(const DepthTexture2D& dTex) = delete;
		DepthTexture2D(DepthTexture2D&& dTex) noexcept;
		//Copy and move assignment operators
		DepthTexture2D& operator=(const DepthTexture2D&) = delete;
		DepthTexture2D& operator=(DepthTexture2D&& tex) noexcept;
		//
		virtual TextureClass GetClass() const override { return TextureClass::DepthTexture2D; }
		//
	private:
		DepthFormat depthFormat = DepthFormat::Depth16BitFloat;
	};
}
