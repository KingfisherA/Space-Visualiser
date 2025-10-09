#pragma once
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"OpenGL Context.hpp"
#include"SV-Engine/Utilities/String.hpp"
#include<utility>
#include<initializer_list>
#include<unordered_map>
#include"SV-Engine/OpenGL/Texture.hpp"

#ifdef SV_DEBUG
#define FBO_DEBUG_VERBOSE
#endif // SV_DEBUG

namespace SV
{
	//Forward Declarations
	class ITexture;
	class Texture2D;
	class Texture3D;

	class RenderBuffer
	{
	public:
		RenderBuffer(unsigned int width, unsigned int height, DepthFormat dFormat, unsigned int samples = 0);
		RenderBuffer(unsigned int width, unsigned int height, TexFormat cFormat, unsigned int samples = 0);
		RenderBuffer(GLuint rBufID);
		RenderBuffer(const RenderBuffer& rBuffer) noexcept;
		RenderBuffer(RenderBuffer&& rBuffer) noexcept;
		~RenderBuffer();
		//
		inline void Bind() const
		{
			glBindRenderbuffer(GL_RENDERBUFFER, renderBufID);
		}
		inline static void Unbind()
		{
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}
		inline const GLuint GetID() const { return renderBufID; }
		inline const bool IsMultisampled() const { return multisampled; }
		//
	private:
		GLuint renderBufID = 0;
		bool multisampled = false;
	};

	//
#pragma region FBO Constants
//
/// <summary>
/// The maximum amount of attachments an FBO can have for one attachment type
/// </summary>
	constexpr unsigned int MaxFBOAttachments = 16;
	//
#pragma endregion
//
	typedef unsigned int AttachmentSlot;
	//
#pragma region FBO Enumerations
	//
	enum class StencilFormat
	{
		//
		DepthStencil = GL_DEPTH_STENCIL,
		//
		Depth24BitInteger_Stencil8BitInteger = GL_DEPTH24_STENCIL8,
		//
		Depth32BitFloat_Stencil8BitInteger = GL_DEPTH32F_STENCIL8
	};
	//
	enum class BufferBits
	{
		//Copies all the avaliable buffer bits in the source FBO
		All = 0x00000001,
		ColourBufferBit = GL_COLOR_BUFFER_BIT,
		DepthBufferBit = GL_DEPTH_BUFFER_BIT,
		StencilBufferBit = GL_STENCIL_BUFFER_BIT
	};
	enum class InterpolationMode
	{
		Linear = GL_LINEAR,
		Nearest = GL_NEAREST
	};
	inline BufferBits operator|(const BufferBits bb1, const BufferBits bb2)
	{
		return BufferBits((unsigned int)(bb1) | (unsigned int)(bb2));
	}
	inline unsigned int operator&(const BufferBits bb1, const BufferBits bb2)
	{
		return (unsigned int)(bb1) & (unsigned int)(bb2);
	}
	//
#pragma endregion


	using ColourTexMap = std::unordered_map<unsigned int, Ref<SV::ITexture>>;
	using ColourBufMap = std::unordered_map<unsigned int, Ref<SV::RenderBuffer>>;

	/// <summary>
	/// A class representing an OpenGL framebuffer object
	/// </summary>
	class FBO
	{
	public:
		/// <summary>
		/// Create a FBO
		/// </summary>
		FBO();
		//
		virtual ~FBO();
		//
		/// <summary>
		/// Forcefully bind this FBO, regardless if it has already been bound
		/// </summary>
		void ForceBind() const
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboID);
			OpenGLContext::currentFBO = this;
		}
		/// <summary>
		/// Bind this FBO
		/// </summary>
		void Bind() const;
	private:
		/// <summary>
		/// Bind this FBO
		/// </summary>
		void BindInternal() const;
	public:
		/// <summary>
		/// Unbind every FBO
		/// </summary>
		static void Unbind()
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			OpenGLContext::currentFBO = nullptr;
		}
		//
		void AddColourAttachment(AttachmentSlot slot, Ref<ITexture> cTexture);
		void AddColourAttachment(AttachmentSlot slot, Ref<RenderBuffer> cBuffer);
		//
		void RemoveColourAttachment(AttachmentSlot slot);
		void RemoveColourAttachment(const Ref<ITexture>& cTexture);
		void RemoveColourAttachment(const Ref<RenderBuffer>& cBuffer);
		//
		void AddDepthAttachment(Ref<DepthTexture2D> dTexture);
		void AddDepthAttachment(Ref<RenderBuffer> dBuffer);
		//
		void RemoveDepthAttachment();
		//
		unsigned int FindAttachmentSlot(const Ref<ITexture>& cTexture) const;
		unsigned int FindAttachmentSlot(const Ref<RenderBuffer>& cBuffer) const;
		//
		void DrawBuffer(int buffer);
		void DrawBuffers(std::initializer_list<int> buffersToDraw);
		//
		void Reset();
		//
		void ReadBuffer(int bufferToRead);
		//
		inline const Ref<ITexture> GetColourTexture(AttachmentSlot slot) const
		{
			if (auto kvPair = colourTextureAttachments.find(slot); kvPair != colourTextureAttachments.end())
			{
				return kvPair->second;
			}
			return nullptr;
		}
		inline const Ref<RenderBuffer> GetColourBuffer(AttachmentSlot slot)const
		{
			if (auto kvPair = colourBufferAttachments.find(slot); kvPair != colourBufferAttachments.end())
			{
				return kvPair->second;
			}
			return nullptr;
		}
		inline const Ref<SV::DepthTexture2D> GetDepthTexture() const { return depthTexture; }
		inline const Ref<RenderBuffer> GetDepthBuffer() const { return depthBuffer; }
		//
		inline void BlitThis(const FBO& destFBO, const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All) { Blit(*this, destFBO, interpolationMode, componentToCopy); }
		inline void BlitThis(const FBO& destFBO, const glm::uivec4& sourceRectangle, const glm::uivec4& destinationRectangle, const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All) { Blit(*this, destFBO, sourceRectangle, destinationRectangle, interpolationMode, componentToCopy); }
		inline void BlitThis(const FBO& destFBO, const glm::uivec4& sourceRectangle, const glm::uivec4& destinationRectangle, const std::initializer_list<AttachmentSlot>& readBuffers,
			const std::initializer_list<AttachmentSlot>& writeBuffers, const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All) {
			Blit(*this, destFBO, sourceRectangle, destinationRectangle, readBuffers, writeBuffers, interpolationMode, componentToCopy);
		}
		inline void BlitThis(const FBO& destFBO, const std::initializer_list<AttachmentSlot>& readBuffers, const std::initializer_list<AttachmentSlot>& writeBuffers,
			const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All) {
			Blit(*this, destFBO, readBuffers, writeBuffers, interpolationMode, componentToCopy);
		}
		//
		static void Blit(const FBO& sourceFBO, const FBO& destFBO, const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All);
		static void Blit(const FBO& sourceFBO, const FBO& destFBO, const glm::uivec4& sourceRectangle, const glm::uivec4& destinationRectangle,
			const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All);
		static void Blit(const FBO& sourceFBO, const FBO& destFBO, const glm::uivec4& sourceRectangle, const glm::uivec4& destinationRectangle, const std::initializer_list<AttachmentSlot>& readBuffers,
			const std::initializer_list<AttachmentSlot>& writeBuffers, const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All);
		static void Blit(const FBO& sourceFBO, const FBO& destFBO, const std::initializer_list<AttachmentSlot>& readBuffers, const std::initializer_list<AttachmentSlot>& writeBuffers,
			const InterpolationMode interpolationMode = InterpolationMode::Nearest, BufferBits componentToCopy = BufferBits::All);
		//
		inline const Dimension2D& GetDimension() const { return fboDimension; }
		inline unsigned int GetWidth() const { return fboDimension.width; }
		inline unsigned int GetHeight() const { return fboDimension.height; }
		//
		inline static void SetViewport(const glm::uivec4& viewportDimensions)
		{
			glViewport(viewportDimensions.x, viewportDimensions.y, viewportDimensions.z, viewportDimensions.w);
		}
		//
		/// <summary>
		/// Get this FBO ID
		/// </summary>
		/// <returns>The FBO ID</returns>
		inline const GLuint GetFBO() const { return fboID; }
		//
		inline const bool UsesMultisampling() const { return multisampled; }
		/// <summary>
		/// Get the FBO's status
		/// </summary>
		/// <returns>The FBO's status</returns>
		inline GLenum GetFramebufferStatus() const
		{
			return glCheckNamedFramebufferStatus(fboID, GL_FRAMEBUFFER);
		}
		/// <summary>
		/// Get a boolean showing if the FBO is usable or not
		/// </summary>
		/// <returns>If the FBO is OK</returns>
		inline bool IsFramebufferComplete() const
		{
			return GetFramebufferStatus() == GL_FRAMEBUFFER_COMPLETE;
		}
		//
		inline SV::String GetFramebufferStatusString() const
		{
			return FramebufferStatusToString(GetFramebufferStatus());
		}
		/// <summary>
		/// Get the FBO's status in a string representation
		/// </summary>
		/// <returns>The FBO's status as a string</returns>
		static SV::String FramebufferStatusToString(GLenum status)
		{
			switch (status)
			{
			case GL_FRAMEBUFFER_COMPLETE:
				return "The framebuffer is OK. (GL_FRAMEBUFFER_COMPLETE)";
				break;

			case GL_FRAMEBUFFER_UNDEFINED:
				return "The framebuffer is not defined (GL_FRAMEBUFFER_UNDEFINED)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				return "The framebuffer does not support the attachment arrangement or it is incomplete (GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				return "The framebuffer is missing a vital attachment. Please ensure at least one valid image or renderbuffer is attached to the FBO. (GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
				return "The framebuffer is missing a draw buffer (GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
				return "The framebuffer is missing a read buffer (GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)";
				break;

			case GL_FRAMEBUFFER_UNSUPPORTED:
				return "The framebuffer arrangement is unsupported. This may be because of: OpenGL version, texture size or unsupported attachmet arrangement (GL_FRAMEBUFFER_UNSUPPORTED)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
				return "The framebuffer's multi-sample numbers are not equal. All multi-sample attachments need to use the same sample amount (GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)";
				break;

			case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
				return "Only some of the framebuffer's textured are layered. All or none need to be layered. (GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)";
				break;

			default:
				return "An unknown error occured";
				break;
			}
		}
		//
	private:
		//Dimensions of this framebuffer
		Dimension2D fboDimension;

		//Does the framebuffer use multisampling?
		bool multisampled = false;

		//The FBO's colour texture and or colour buffer attachments
		mutable ColourTexMap colourTextureAttachments;
		mutable ColourBufMap colourBufferAttachments;

		//The FBO's depth texture attachment
		Ref<SV::DepthTexture2D> depthTexture;
		//The FBO's depth buffer attachment
		Ref<SV::RenderBuffer> depthBuffer;

		//The ID of this FBO
		GLuint fboID = 0;
	};

}
