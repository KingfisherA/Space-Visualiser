#pragma once
#include"SV/Utilities/GlobalItems.hpp"
#include"OpenGL Context.hpp"
#include"SV/Utilities/SVMemory.hpp"
#include"SV/Utilities/Array.hpp"

//extern "C"

namespace SV
{
	struct AttributeLayout
	{
	public:
		AttributeLayout() {}
		AttributeLayout(unsigned char eleSize) : elementSize(eleSize) {}
		AttributeLayout(unsigned char eleSize, unsigned int strideIn) : elementSize(eleSize), stride(strideIn) {}
		AttributeLayout(unsigned char eleSize, unsigned int strideIn, unsigned int pointerIn) : elementSize(eleSize), stride(strideIn), pointer(pointerIn) {}
		AttributeLayout(unsigned char eleSize, unsigned int strideIn, unsigned int pointerIn, unsigned short divisorIn) : elementSize(eleSize), stride(strideIn), pointer(pointerIn), divisor(divisorIn) {}

		//Size of one element
		unsigned char elementSize = 3;
		//In elements
		unsigned int stride = 0;
		//In elements
		unsigned int pointer = 0;
		//
		unsigned short divisor = 0;
	};

	class iVBO
	{
	public:
		virtual ~iVBO() {}
		//
		virtual void Bind() = 0;
		virtual void Unbind() = 0;
		//
		inline unsigned int GetID() { return vboID; }
		inline unsigned int GetByteSize() { return vboByteSize; }
		inline unsigned int GetCount() { return vboCount; }
		inline GLType GetPrimitiveType() { return primitiveType; }
		inline void ReinterpretType(GLType type) { primitiveType = type; }
		//
	protected:
		//
		unsigned int vboID = 0;
		//Byte Size of buffer
		unsigned int vboByteSize = 0;
		unsigned int vboCount = 0;
		//
		GLType primitiveType = GLType::Null;
	};

	enum class BufferUsage
	{
		StaticDraw = GL_STATIC_DRAW,
		DynamicDraw = GL_DYNAMIC_DRAW
	};

	class ArrayBuffer : public iVBO
	{
	public:
		//
		static Array<Ref<ArrayBuffer>> GenerateBuffers(unsigned int number);
		//
		ArrayBuffer();
		ArrayBuffer(GLuint ID);
		ArrayBuffer(const float* data, unsigned int count, BufferUsage usage = BufferUsage::StaticDraw);
		ArrayBuffer(const ArrayView<float>& data, BufferUsage usage = BufferUsage::StaticDraw);
		ArrayBuffer(unsigned int byteSize, GLType type, BufferUsage usage = BufferUsage::StaticDraw);
		ArrayBuffer(const ArrayBuffer& vboIn) = delete;
		//
		virtual ~ArrayBuffer();
		//
		void StoreData(const ArrayView<float>& data, BufferUsage usage = BufferUsage::StaticDraw)
		{
			SV_ASSERT_T(data == nullptr, SV::ArrayBuffer::StoreData, "Cannot store data as data is nullptr.");
			ArrayBuffer::StoreData(data.GetInternalArray(), (unsigned int)data.GetCount(), usage);
		}
		void StoreData(const float* data, unsigned int count, BufferUsage usage = BufferUsage::StaticDraw);
		//
		void UpdateData(ArrayView<float> data, unsigned int offset = 0)
		{
			SV_ASSERT_T(data == nullptr, SV::ArrayBuffer::UpdateData, "Cannot update data as data is nullptr.");
			ArrayBuffer::UpdateData(data.GetInternalArray(), (unsigned int)data.GetCount(), offset);
		}
		void UpdateData(const float* data, unsigned int count, unsigned int offset);
		//
		virtual void Bind() override
		{
			glBindBuffer(GL_ARRAY_BUFFER, vboID);
			OpenGLContext::currentVBO = this;
		}
		virtual void Unbind() override
		{
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			OpenGLContext::currentVBO = nullptr;
		}
		//
	};

	class IndexBuffer : public iVBO
	{
	public:
		//
		static Array<Ref<IndexBuffer>> GenerateBuffers(unsigned int number);
		//
		IndexBuffer();
		IndexBuffer(GLuint ID);
		IndexBuffer(const ArrayView<unsigned int>& data, BufferUsage usage = BufferUsage::StaticDraw);
		IndexBuffer(unsigned int byteSize, BufferUsage usage);
		IndexBuffer(ArrayBuffer& vboIn) = delete;
		//
		virtual ~IndexBuffer();
		//
		void StoreData(const ArrayView<unsigned int>& data, BufferUsage usage = BufferUsage::StaticDraw);
		void UpdateData(const ArrayView<unsigned int>& data, unsigned int offset);
		//
		virtual void Bind() override
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboID);
			OpenGLContext::currentVBO = this;
		}
		virtual void Unbind() override
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			OpenGLContext::currentVBO = nullptr;
		}
		//
	};
}