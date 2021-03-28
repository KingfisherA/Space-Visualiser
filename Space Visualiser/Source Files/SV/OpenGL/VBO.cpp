#include"SV/Precompiled Global.hpp"
#include"VBO.hpp"
#include"SV/Utilities/Array.hpp"
#include"SV/Utilities/Errors.hpp"
#include<type_traits>

namespace SV
{
	//
	//
#pragma region Array Buffer
//
	Array<Ref<ArrayBuffer>> ArrayBuffer::GenerateBuffers(unsigned int number)
	{
		GLuint* buffers = (GLuint*)_malloca(number * sizeof(GLuint));
		SV_ASSERT_T(buffers == nullptr, SV::ArrayBuffer::GenerateBuffers, "Could not allocate memory for ArrayBuffer::GenerateBuffers");
		if (buffers == nullptr) throw std::overflow_error("Could not allocate memory for ArrayBuffer::GenerateBuffers");
		glCreateBuffers(number, buffers);
		Array<Ref<ArrayBuffer>> vbos(number, true);

		for (unsigned int i = 0; i < number; i++)
		{
			vbos.Set(i,
				create<ArrayBuffer>(buffers[i]));
		}

		_freea(buffers);

		return std::move(vbos);
	}
	//
	ArrayBuffer::ArrayBuffer()
	{
		glCreateBuffers(1, &vboID);
	}
	//
	ArrayBuffer::ArrayBuffer(GLuint ID)
	{
		vboID = ID;
	}
	//
	ArrayBuffer::ArrayBuffer(const float* data, unsigned int count, BufferUsage usage)
	{
		glCreateBuffers(1, &vboID);
		StoreData(data, count, usage);
	}
	//
	ArrayBuffer::ArrayBuffer(const ArrayView<float>& data, BufferUsage usage)
	{
		glCreateBuffers(1, &vboID);
		StoreData(data, usage);
	}
	//
	ArrayBuffer::ArrayBuffer(unsigned int byteSize, GLType type, BufferUsage usage)
	{
		glCreateBuffers(1, &vboID);
		this->Bind();
		glBufferData(GL_ARRAY_BUFFER, byteSize, nullptr, (GLenum)usage);

		vboByteSize = byteSize;
		vboCount = byteSize / OpenGLContext::TypeToByteSize(type);
		primitiveType = type;
	}
	//
	ArrayBuffer::~ArrayBuffer()
	{
		glDeleteBuffers(1, &vboID);
	}
	//
	void ArrayBuffer::StoreData(const float* data, unsigned int count, BufferUsage usage)
	{
		this->Bind();
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(float), (const void*)data, (GLenum)usage);

		vboByteSize = count * sizeof(float);
		vboCount = count;
		primitiveType = GLType::Float;
	}
	//
	void ArrayBuffer::UpdateData(const float* data, unsigned int count, unsigned int offset)
	{
		count *= sizeof(float);
		if (count + offset > vboByteSize) count = vboByteSize;

		this->Bind();
		glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offset, count, (const void*)data);
	}
	//
#pragma endregion
//
#pragma region Index Buffer
//
	Array<Ref<IndexBuffer>> IndexBuffer::GenerateBuffers(unsigned int number)
	{
		GLuint* buffers = (GLuint*)_malloca(number * sizeof(GLuint));
		SV_ASSERT_T(buffers == nullptr, SV::IndexBuffer::GenerateBuffers, "Could not allocate memory for IndexBuffer::GenerateBuffers");
		if (buffers == nullptr) throw std::overflow_error("Could not allocate memory for IndexBuffer::GenerateBuffers");

		glCreateBuffers(number, buffers);
		Array<Ref<IndexBuffer>> ebos(number, true);

		for (unsigned int i = 0; i < number; i++)
		{
			ebos.Set(i, create<IndexBuffer>(buffers[i]));
		}

		_freea(buffers);

		return std::move(ebos);
	}
	//
	IndexBuffer::IndexBuffer()
	{
		glCreateBuffers(1, &vboID);
	}
	//
	IndexBuffer::IndexBuffer(GLuint ID)
	{
		vboID = ID;
	}
	//
	IndexBuffer::IndexBuffer(const ArrayView<unsigned int>& data, BufferUsage usage)
	{
		glCreateBuffers(1, &vboID);
		StoreData(data, usage);
	}
	//
	IndexBuffer::IndexBuffer(unsigned int byteSize, BufferUsage usage)
	{
		this->Bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, byteSize, nullptr, (GLenum)usage);

		vboByteSize = byteSize;
		vboCount = byteSize / sizeof(unsigned int);
		primitiveType = GLType::UInt;
	}
	//
	IndexBuffer::~IndexBuffer()
	{
		glDeleteBuffers(1, &vboID);
	}
	//
	void IndexBuffer::StoreData(const SV::ArrayView<unsigned int>& data, BufferUsage usage)
	{
		SV_ASSERT_T(data == nullptr, SV::IndexBuffer::StoreData, "Cannot store data as data is nullptr.");

		this->Bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.GetCount() * sizeof(unsigned int), (const void*)data.GetInternalArray(), (GLenum)usage);

		vboByteSize = (unsigned int)data.GetCount() * sizeof(unsigned int);
		vboCount = (unsigned int)data.GetCount();
		primitiveType = GLType::UInt;
	}
	//
	void IndexBuffer::UpdateData(const SV::ArrayView<unsigned int>& data, unsigned int offset)
	{
		SV_ASSERT_T(data == nullptr, SV::IndexBuffer::UpdateData, "Cannot update data as data is nullptr.");

		this->Bind();
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)offset, data.GetCount() * sizeof(unsigned int), (const void*)data.GetInternalArray());
	}
	//
#pragma endregion
//
}