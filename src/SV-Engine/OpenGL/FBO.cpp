#include"SV-Engine/Precompiled Global.hpp"
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"SV-Engine/OpenGL/FBO.hpp"
#include"SV-Engine/OpenGL/Texture.hpp"
#include<numeric>

namespace SV
{
	//
#pragma region RenderBuffer Class
	//
	RenderBuffer::RenderBuffer(unsigned int width, unsigned int height, DepthFormat dFormat, unsigned int samples)
	{
		glGenRenderbuffers(1, &renderBufID);
		glBindRenderbuffer(GL_RENDERBUFFER, renderBufID);

		multisampled = samples > 0;

		if (multisampled)
		{
#ifdef SV_DEBUG
			if (!glIsEnabled(GL_MULTISAMPLE)) SV_WARN_T(SV::RenderBuffer::RenderBuffer, "Note that GL_MULTISAMPLE is not enabled. To use multisapled render buffers, GL_MULTISAMPLE must be enabled.");
#endif // SV_DEBUG

			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GLenum(dFormat), width, height);
		}
		else
		{
			glRenderbufferStorage(GL_RENDERBUFFER, GLenum(dFormat), width, height);
		}

		glBindRenderbuffer(renderBufID, 0);
	}
	//
	RenderBuffer::RenderBuffer(unsigned int width, unsigned int height, TexFormat cFormat, unsigned int samples)
	{
		glGenRenderbuffers(1, &renderBufID);
		glBindRenderbuffer(GL_RENDERBUFFER, renderBufID);
		multisampled = samples > 0;

		if (multisampled)
		{
#ifdef SV_DEBUG
			if (!glIsEnabled(GL_MULTISAMPLE)) SV_WARN_T(SV::RenderBuffer::RenderBuffer, "Note that GL_MULTISAMPLE is not enabled. To use multisapled render buffers, GL_MULTISAMPLE must be enabled.");
#endif // SV_DEBUG

			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GLenum(cFormat), width, height);
		}
		else
		{
			glRenderbufferStorage(GL_RENDERBUFFER, GLenum(cFormat), width, height);
		}

		glBindRenderbuffer(renderBufID, 0);
	}
	//
	RenderBuffer::RenderBuffer(GLuint rBufID)
	{
		renderBufID = rBufID;
	}
	//
	RenderBuffer::RenderBuffer(const RenderBuffer& rBuffer) noexcept
	{
		renderBufID = rBuffer.renderBufID;
	}
	//
	RenderBuffer::RenderBuffer(RenderBuffer&& rBuffer) noexcept
	{
		renderBufID = rBuffer.renderBufID;
		rBuffer.renderBufID = 0;
	}
	//
	RenderBuffer::~RenderBuffer()
	{
		glDeleteRenderbuffers(1, &renderBufID);
	}
	//
#pragma endregion
	//
#pragma region FBO Class
	//
	constexpr Dimension2D MAX_FBO_SIZE = { std::numeric_limits<unsigned int>::max(), std::numeric_limits<unsigned int>::max() };
	//
	FBO::FBO() : fboDimension(MAX_FBO_SIZE)
	{
		glGenFramebuffers(1, &fboID);
	}
	//
	FBO::~FBO()
	{
		colourTextureAttachments.clear();
		colourBufferAttachments.clear();

		glDeleteFramebuffers(1, &fboID);
		fboID = 0;
	}
	//
	void FBO::Bind() const
	{
		if (OpenGLContext::currentFBO != this)
		{
#ifdef FBO_DEBUG_VERBOSE
			GLenum errorCode = this->GetFramebufferStatus();
			if (errorCode != GL_FRAMEBUFFER_COMPLETE)
			{
				SV_WARN_T(SV::FBO::Bind, (" Warning; Framebuffer " + SV::NumberToString(this->fboID) + " is not complete. Please check the framebuffer has been setup according to the OpenGL specification correctly. \nLog: " + FramebufferStatusToString(errorCode)).GetStringArray());
			}
#endif
			glBindFramebuffer(GL_FRAMEBUFFER, fboID);
			OpenGLContext::currentFBO = this;
		}
	}
	//
	void FBO::BindInternal() const
	{
		if (OpenGLContext::currentFBO != this)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboID);
			OpenGLContext::currentFBO = this;
		}
	}
	//
	void FBO::AddColourAttachment(AttachmentSlot slot, Ref<ITexture> cTexture)
	{
		SV_ASSERT_T(cTexture == nullptr, SV::FBO::AddColourAttachment, "Colour texture inputted is nullptr");
		SV_ASSERT_WARN_T(colourBufferAttachments.find(slot) != colourBufferAttachments.end(), SV::FBO::AddColourAttachment, ("Addition of colour texture in slot " + SV::NumberToString(slot) + " will overwrite colour buffer.").GetStringArray());

		this->BindInternal();
		if (colourBufferAttachments.find(slot) != colourBufferAttachments.end())
		{
			colourBufferAttachments.erase(slot);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_RENDERBUFFER, 0);
		}

		if (cTexture->GetDimension() <= fboDimension || fboDimension == MAX_FBO_SIZE)
		{
			fboDimension = cTexture->GetDimension();
		}
#ifdef FBO_DEBUG_VERBOSE
		else
		{
			SV_WARN_T(SV::FBO::AddColourAttachment, ("Note that a new texture attached to framebuffer ID=" + SV::NumberToString(fboID) + " in attachment slot \"" + SV::NumberToString(slot) + "\" has a different size to previous textures. Note that the FBO will only render to at the resolution of the lowest resolution texture attached.").GetStringArray());
		}

		if (multisampled && !cTexture->IsMultisampled())
		{
			SV_WARN_T(SV::FBO::AddColourAttachment, ("Note that a new texture attached to framebuffer ID=" + SV::NumberToString(fboID) + " in attachment slot \"" + SV::NumberToString(slot) + "\" is not multisampled, but another texture/renderbuffer in the FBO is.").GetStringArray());
		}
#endif

		multisampled |= cTexture->IsMultisampled();

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, cTexture->GetTextureID(), 0);

		colourTextureAttachments[slot] = cTexture;
	}
	//
	void FBO::AddColourAttachment(AttachmentSlot slot, Ref<RenderBuffer> cBuffer)
	{
		SV_ASSERT_T(cBuffer == nullptr, SV::FBO::AddColourAttachment, "Colour buffer inputted is nullptr");
		SV_ASSERT_WARN_T(colourTextureAttachments.find(slot) != colourTextureAttachments.end(), SV::FBO::AddColourAttachment, ("Addition of colour buffer in slot " + SV::NumberToString(slot) + " will overwrite colour texture.").GetStringArray());

		this->BindInternal();
		if (colourTextureAttachments.find(slot) != colourTextureAttachments.end())
		{
			colourTextureAttachments.erase(slot);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, 0, 0);
		}


		multisampled |= cBuffer->IsMultisampled();
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_RENDERBUFFER, cBuffer->GetID());

		colourBufferAttachments[slot] = cBuffer;

#ifdef FBO_DEBUG_VERBOSE
		GLenum errorCode = this->GetFramebufferStatus();
		if (errorCode != GL_FRAMEBUFFER_COMPLETE)
		{
			SV_WARN_T(SV::FBO::AddColourAttachment, (" Warning; Framebuffer " + SV::NumberToString(this->fboID) + " is not complete. Please check the framebuffer has been setup according to the OpenGL specification correctly. \nLog: " + FramebufferStatusToString(errorCode)).GetStringArray());
		}

		if (multisampled && !cBuffer->IsMultisampled())
		{
			SV_WARN_T(SV::FBO::AddColourAttachment, ("Note that a new renderbuffer attached to framebuffer ID=" + SV::NumberToString(fboID) + " in attachment slot \"" + SV::NumberToString(slot) + "\" is not multisampled, but another texture/renderbuffer in the FBO is.").GetStringArray());
		}
#endif
	}
	//
	void FBO::RemoveColourAttachment(AttachmentSlot slot)
	{
		if (colourTextureAttachments.find(slot) != colourTextureAttachments.end())
		{
			this->BindInternal();
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, 0, 0);

			colourTextureAttachments.erase(slot);
		}
		else if (colourBufferAttachments.find(slot) != colourBufferAttachments.end())
		{
			this->BindInternal();
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_RENDERBUFFER, 0);

			colourBufferAttachments.erase(slot);
		}
#ifdef FBO_DEBUG_VERBOSE
		else
		{
			SV_FATAL_T(FBO::RemoveColourAttachment, ("Texture at slot \"" + SV::NumberToString(slot) + "\" is out of range, hence cannot be removed.").GetStringArray());
		}
#endif // FBO_DEBUG_VERBOSE
	}
	//
	void FBO::RemoveColourAttachment(const Ref<ITexture>& cTexture)
	{
		unsigned int slot = FBO::FindAttachmentSlot(cTexture);
		if (slot != IndexUndefined)
		{
			this->BindInternal();
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, 0, 0);

			colourTextureAttachments.erase(slot);
		}
#ifdef FBO_DEBUG_VERBOSE
		else
		{
			SV_FATAL_T(FBO::RemoveColourAttachment, ("Texture at slot \"" + SV::NumberToString(slot) + "\" is out of range, hence cannot be removed.").GetStringArray());
		}
#endif // FBO_DEBUG_VERBOSE
	}
	//
	void FBO::RemoveColourAttachment(const Ref<RenderBuffer>& cBuffer)
	{
		unsigned int slot = FBO::FindAttachmentSlot(cBuffer);
		if (slot != IndexUndefined)
		{
			this->BindInternal();
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_RENDERBUFFER, 0);

			colourBufferAttachments.erase(slot);
		}
#ifdef FBO_DEBUG_VERBOSE
		else
		{
			SV_FATAL_T(FBO::RemoveColourAttachment, ("Texture at slot \"" + SV::NumberToString(slot) + "\" is out of range, hence cannot be removed.").GetStringArray());
		}
#endif // FBO_DEBUG_VERBOSE
	}
	//
	void FBO::AddDepthAttachment(Ref<SV::DepthTexture2D> dTexture)
	{
		SV_ASSERT_T(dTexture == nullptr, SV::FBO::AddDepthAttachment, "Depth texture inputted is nullptr");
		SV_ASSERT_WARN_T(depthBuffer != nullptr, SV::FBO::AddDepthAttachment, "Addition of depth texture will overwrite depth buffer.");

		this->BindInternal();
		if (depthBuffer != nullptr)
		{
			depthBuffer.reset();
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
		}

		if (fboDimension.width == std::numeric_limits<unsigned int>::max())
		{
			fboDimension = dTexture->GetDimension();
		}

#ifdef FBO_DEBUG_VERBOSE
		if (dTexture->GetDimension() != fboDimension)
		{
			SV_WARN_T(SV::FBO::AddColourAttachment, "Note that the depth texture has a different resolution to the framebuffer");
		}
#endif

		this->BindInternal();
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, dTexture->GetTextureID(), 0);

		depthTexture = dTexture;
	}
	//
	void FBO::AddDepthAttachment(Ref<RenderBuffer> dBuffer)
	{
		SV_ASSERT_T(dBuffer == nullptr, SV::FBO::AddDepthAttachment, "Depth buffer inputted is nullptr");
		SV_ASSERT_WARN_T(depthTexture != nullptr, SV::FBO::AddDepthAttachment, "Addition of depth buffer will overwrite depth texture.");

		this->BindInternal();
		if (depthTexture != nullptr)
		{
			depthTexture.reset();
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0);
		}
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dBuffer->GetID());

		depthBuffer = dBuffer;
	}
	//
	void FBO::RemoveDepthAttachment()
	{
		if (depthTexture != nullptr)
		{
			this->BindInternal();
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0);

			depthTexture.reset();
		}
		else if (depthBuffer != nullptr)
		{
			this->BindInternal();
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);

			depthBuffer.reset();
		}
#ifdef FBO_DEBUG_VERBOSE
		else
		{
			SV_FATAL_T(FBO::RemoveDepthAttachment, "There is no depth buffer or texture bound to the current fbo, thus it cannot be removed.");
		}
#endif // FBO_DEBUG_VERBOSE
	}
	//
	unsigned int FBO::FindAttachmentSlot(const Ref<ITexture>& cTexture) const
	{
		for (auto kvPair : colourTextureAttachments)
		{
			if (kvPair.second.operator->() == cTexture.operator->()) return kvPair.first;
		}
		return IndexUndefined;
	}
	//
	unsigned int FBO::FindAttachmentSlot(const Ref<RenderBuffer>& cBuffer) const
	{
		for (auto kvPair : colourBufferAttachments)
		{
			if (kvPair.second.operator->() == cBuffer.operator->()) return kvPair.first;
		}
		return IndexUndefined;
	}
	//
	void FBO::DrawBuffer(int buffer)
	{
		this->BindInternal();
		glDrawBuffer(buffer < 0 ? GL_NONE : GL_COLOR_ATTACHMENT0 + buffer);

#ifdef FBO_DEBUG_VERBOSE
		GLenum errorCode = this->GetFramebufferStatus();
		if (errorCode != GL_FRAMEBUFFER_COMPLETE)
		{
			SV_WARN_T(SV::FBO::AddDepthAttachment, (" Warning; Framebuffer " + SV::NumberToString(this->fboID) + " is not complete. Please check the framebuffer has been setup according to the OpenGL specification correctly. \nLog: " + FramebufferStatusToString(errorCode)).GetStringArray());
		}
#endif
	}
	//
	void FBO::DrawBuffers(std::initializer_list<int> buffersToDraw)
	{
		if (buffersToDraw.size() == 0)
		{
			glDrawBuffer(GL_NONE);
			return;
		}

		if ((*buffersToDraw.begin()) < 0)
		{
			this->BindInternal();

			glDrawBuffer(GL_NONE);
			return;
		}

		int* attachmentArray = const_cast<int*>(buffersToDraw.begin());
		for (size_t i = 0; i < buffersToDraw.size(); i++)
		{
			attachmentArray[i] = GL_COLOR_ATTACHMENT0 + attachmentArray[i];
		}

		this->BindInternal();
		glDrawBuffers((GLsizei)buffersToDraw.size(), (const GLenum*)attachmentArray);

#ifdef FBO_DEBUG_VERBOSE
		GLenum errorCode = this->GetFramebufferStatus();
		if (errorCode != GL_FRAMEBUFFER_COMPLETE)
		{
			SV_WARN_T(SV::FBO::AddDepthAttachment, (" Warning; Framebuffer " + SV::NumberToString(this->fboID) + " is not complete. Please check the framebuffer has been setup according to the OpenGL specification correctly. \nLog: " + FramebufferStatusToString(errorCode)).GetStringArray());
		}
#endif
	}
	//
	void FBO::Reset()
	{
		this->BindInternal();
		unsigned int i = 0;

		for (; i < colourTextureAttachments.size(); i++)
		{
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, 0, 0);
		}
		colourTextureAttachments.clear();

		for (; i < colourBufferAttachments.size(); i++)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, 0, 0);
		}
		colourBufferAttachments.clear();

		if (depthTexture != nullptr)
		{
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0);
			depthTexture.reset();
		}
		else if (depthBuffer != nullptr)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			depthBuffer.reset();
		}

		fboDimension = { std::numeric_limits<unsigned int>::max(), std::numeric_limits<unsigned int>::max() };
		multisampled = false;
	}
	//
	void FBO::ReadBuffer(int bufferToRead)
	{
		this->BindInternal();

		if (bufferToRead < 0)
		{
			glReadBuffer(GL_NONE);
			return;
		}

		glReadBuffer(GL_COLOR_ATTACHMENT0 + bufferToRead);

#ifdef FBO_DEBUG_VERBOSE
		GLenum errorCode = this->GetFramebufferStatus();
		if (errorCode != GL_FRAMEBUFFER_COMPLETE)
		{
			SV_WARN_T(SV::FBO::AddDepthAttachment, (" Warning; Framebuffer " + SV::NumberToString(this->fboID) + " is not complete. Please check the framebuffer has been setup according to the OpenGL specification correctly. \nLog: " + FramebufferStatusToString(errorCode)).GetStringArray());
		}
#endif
	}
	//
	void FBO::Blit(const FBO& sourceFBO, const FBO& destFBO, const InterpolationMode interpolationMode, BufferBits componentToCopy)
	{
#ifdef FBO_DEBUG_VERBOSE
		if (!sourceFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The source buffer (ID=" + SV::NumberToString(sourceFBO.GetFBO()) + ") is incomplete. Cannot blit from an incomplete framebuffer. Log: \n" + sourceFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
		if (!destFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The destination buffer (ID=" + SV::NumberToString(destFBO.GetFBO()) + ") is incomplete. Cannot blit to an incomplete framebuffer. Log: \n" + destFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
#endif // FBO_DEBUG_VERBOSE

		glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO.fboID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO.fboID);

		const Dimension2D& sourceDim = sourceFBO.fboDimension;
		const Dimension2D& destDim = destFBO.fboDimension;

		if (componentToCopy & BufferBits::All)
		{
			componentToCopy = (BufferBits)0;
			if (sourceFBO.colourBufferAttachments.size() > 0 || sourceFBO.colourTextureAttachments.size() > 0)
			{
				componentToCopy = componentToCopy | BufferBits::ColourBufferBit;
			}
			if (sourceFBO.depthTexture != nullptr || sourceFBO.depthBuffer != nullptr)
			{
				componentToCopy = componentToCopy | BufferBits::DepthBufferBit;
			}
		}

		unsigned int amount = (unsigned int)glm::min(glm::min(sourceFBO.colourBufferAttachments.size(), sourceFBO.colourTextureAttachments.size()), glm::min(sourceFBO.colourBufferAttachments.size(), sourceFBO.colourTextureAttachments.size()));
		for (unsigned int i = 0; i < amount; i++)
		{
			glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
			glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);

			glBlitFramebuffer(0, 0, sourceDim.width, sourceDim.height, 0, 0, destDim.width, destDim.height, (GLbitfield)componentToCopy, (GLenum)interpolationMode);
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
	void FBO::Blit(const FBO& sourceFBO, const FBO& destFBO, const glm::uivec4& sourceRectangle, const glm::uivec4& destinationRectangle, const InterpolationMode interpolationMode, BufferBits componentToCopy)
	{
#ifdef FBO_DEBUG_VERBOSE
		if (!sourceFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The source buffer (ID=" + SV::NumberToString(sourceFBO.GetFBO()) + ") is incomplete. Cannot blit from an incomplete framebuffer. Log: \n" + sourceFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
		if (!destFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The destination buffer (ID=" + SV::NumberToString(destFBO.GetFBO()) + ") is incomplete. Cannot blit to an incomplete framebuffer. Log: \n" + destFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
#endif // FBO_DEBUG_VERBOSE

		glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO.fboID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO.fboID);

		if (componentToCopy & BufferBits::All)
		{
			componentToCopy = (BufferBits)0;
			if (sourceFBO.colourBufferAttachments.size() > 0 || sourceFBO.colourTextureAttachments.size() > 0)
			{
				componentToCopy = componentToCopy | BufferBits::ColourBufferBit;
			}
			if (sourceFBO.depthTexture != nullptr || sourceFBO.depthBuffer != nullptr)
			{
				componentToCopy = componentToCopy | BufferBits::DepthBufferBit;
			}
		}

		unsigned int amount = (unsigned int)glm::min(glm::min(sourceFBO.colourBufferAttachments.size(), sourceFBO.colourTextureAttachments.size()), glm::min(sourceFBO.colourBufferAttachments.size(), sourceFBO.colourTextureAttachments.size()));
		for (unsigned int i = 0; i < amount; i++)
		{
			glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
			glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);

			glBlitFramebuffer(sourceRectangle.x, sourceRectangle.y, sourceRectangle.z, sourceRectangle.w, destinationRectangle.x, destinationRectangle.y, destinationRectangle.z, destinationRectangle.w, (GLbitfield)componentToCopy, (GLenum)interpolationMode);
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
	void FBO::Blit(const FBO& sourceFBO, const FBO& destFBO, const glm::uivec4& sourceRectangle, const glm::uivec4& destinationRectangle,
		const std::initializer_list<AttachmentSlot>& readBuffers, const std::initializer_list<AttachmentSlot>& writeBuffers, const InterpolationMode interpolationMode, BufferBits componentToCopy)
	{
#ifdef FBO_DEBUG_VERBOSE
		if (!sourceFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The source buffer (ID=" + SV::NumberToString(sourceFBO.GetFBO()) + ") is incomplete. Cannot blit from an incomplete framebuffer. Log: \n" + sourceFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
		if (!destFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The destination buffer (ID=" + SV::NumberToString(destFBO.GetFBO()) + ") is incomplete. Cannot blit to an incomplete framebuffer. Log: \n" + destFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
#endif // FBO_DEBUG_VERBOSE

		glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO.fboID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO.fboID);

		if (componentToCopy & BufferBits::All)
		{
			componentToCopy = (BufferBits)0;
			if (sourceFBO.colourBufferAttachments.size() > 0 || sourceFBO.colourTextureAttachments.size() > 0)
			{
				componentToCopy = componentToCopy | BufferBits::ColourBufferBit;
			}
			if (sourceFBO.depthTexture != nullptr || sourceFBO.depthBuffer != nullptr)
			{
				componentToCopy = componentToCopy | BufferBits::DepthBufferBit;
			}
		}

		unsigned int amount = (unsigned int)glm::min(readBuffers.size(), writeBuffers.size());
		for (unsigned int i = 0; i < amount; i++)
		{
			glReadBuffer(GL_COLOR_ATTACHMENT0 + readBuffers.begin()[i]);
			glDrawBuffer(GL_COLOR_ATTACHMENT0 + writeBuffers.begin()[i]);

			glBlitFramebuffer(sourceRectangle.x, sourceRectangle.y, sourceRectangle.z, sourceRectangle.w, destinationRectangle.x, destinationRectangle.y, destinationRectangle.z, destinationRectangle.w, (GLbitfield)componentToCopy, (GLenum)interpolationMode);
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
	void FBO::Blit(const FBO& sourceFBO, const FBO& destFBO, const std::initializer_list<AttachmentSlot>& readBuffers, const std::initializer_list<AttachmentSlot>& writeBuffers, const InterpolationMode interpolationMode, BufferBits componentToCopy)
	{
#ifdef FBO_DEBUG_VERBOSE
		if (!sourceFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The source buffer (ID=" + SV::NumberToString(sourceFBO.GetFBO()) + ") is incomplete. Cannot blit from an incomplete framebuffer. Log: \n" + sourceFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
		if (!destFBO.IsFramebufferComplete())
		{
			SV_FATAL_T(FBO::Blit, ("The destination buffer (ID=" + SV::NumberToString(destFBO.GetFBO()) + ") is incomplete. Cannot blit to an incomplete framebuffer. Log: \n" + destFBO.GetFramebufferStatusString().GetStringArray()).GetStringArray());
			return;
		}
#endif // FBO_DEBUG_VERBOSE

		glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO.fboID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO.fboID);

		if (componentToCopy & BufferBits::All)
		{
			componentToCopy = (BufferBits)0;
			if (sourceFBO.colourBufferAttachments.size() > 0 || sourceFBO.colourTextureAttachments.size() > 0)
			{
				componentToCopy = componentToCopy | BufferBits::ColourBufferBit;
			}
			if (sourceFBO.depthTexture != nullptr || sourceFBO.depthBuffer != nullptr)
			{
				componentToCopy = componentToCopy | BufferBits::DepthBufferBit;
			}
		}

		const Dimension2D& sourceDim = sourceFBO.fboDimension;
		const Dimension2D& destDim = destFBO.fboDimension;

		unsigned int amount = (unsigned int)glm::min(readBuffers.size(), writeBuffers.size());
		for (unsigned int i = 0; i < amount; i++)
		{
			glReadBuffer(GL_COLOR_ATTACHMENT0 + readBuffers.begin()[i]);
			glDrawBuffer(GL_COLOR_ATTACHMENT0 + writeBuffers.begin()[i]);

			glBlitFramebuffer(0, 0, sourceDim.width, sourceDim.height, 0, 0, destDim.width, destDim.height, (GLbitfield)componentToCopy, (GLenum)interpolationMode);
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
	//
#pragma endregion
	//
}