#include"SV-Engine/Precompiled Global.hpp"
#include "VAO.hpp"
#include"VBO.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include"SV-Engine/Utilities/Errors.hpp"
#include"SV-Engine/Utilities/String.hpp"
#include<GL/glew.h>
#include<utility>

namespace SV
{
	//
	//
	Array<Ref<VAO>> VAO::GenerateArrays(unsigned int number)
	{
		GLuint* arrays = (GLuint*)alloca(sizeof(GLuint) * number);
		SV_ASSERT_T(arrays == nullptr, SV::VAO::GenerateArrays, "Memory for vital VAO function could not be allocated");
			if (arrays == nullptr) throw std::overflow_error("Memory for vital VAO function could not be allocated");

		glGenVertexArrays(number, arrays);
		Array<Ref<VAO>> vaos(number);
		for (unsigned int i = 0; i < number; i++)
		{
			vaos[i] = create <VAO>(arrays[i]);
		}

		return std::move(vaos);
	}
	//
	VAO::VAO()
	{
		glGenVertexArrays(1, &vaoID);
		//glCreateVertexArrays(1, &vaoID);
	}
	//
	VAO::VAO(VAO& vaoIn)
	{
		glCreateVertexArrays(1, &vaoID);

		memcpy(&buffers[0], &vaoIn.buffers[0], sizeof(buffers));

		renderMethod = vaoIn.renderMethod;
		arrayLength = vaoIn.arrayLength;
	}
	//
	VAO::VAO(GLuint id)
	{
		vaoID = id;
	}
	//
	VAO::~VAO()
	{
		glDeleteVertexArrays(1, &vaoID);
		vaoID = 0;
	}
	//
	void VAO::AddAttributeArray(AttributeArray aa, const Ref<ArrayBuffer> buffer, const AttributeLayout& layout)
	{
		if (buffers.Exists(aa))
		{
			SV_WARN_T(SV::VAO::AddAttributeArray, ("Slot " + SV::NumberToString(aa) + " is already occupied, thus the buffer in it will be deleted.").GetStringArray());
			buffers[aa].reset();
			buffers[aa] = buffer;
		}
		else
		{
			buffers.Add(buffer);
		}

		unsigned char primSize = OpenGLContext::TypeToByteSize(buffer->GetPrimitiveType());

		buffer->Bind();
		this->Bind();
		glVertexAttribPointer(aa, layout.elementSize, (GLenum)buffer->GetPrimitiveType(), GL_FALSE, (GLsizei)layout.stride * primSize, (const GLvoid*)(layout.pointer * (size_t)primSize));
		glVertexAttribDivisor(aa, layout.divisor);

		if (aa == 0)
		{
			if (indexBuffer == nullptr)
			{
				arrayLength = buffer->GetCount();
			}
		}
	}
	//
	void VAO::SetArrayInstanceDivisor(AttributeArray aa, unsigned int divisor)
	{
		this->Bind();
		glVertexAttribDivisor(aa, divisor);
	}
	//
	void VAO::DeleteAttributeArray(AttributeArray aa)
	{
		SV_ASSERT_T(!buffers.Exists(aa), SV::VAO::DeleteAttributeArray, "Cannot delete an empty buffer slot.");
		SV_ASSERT_T(buffers[aa] == nullptr, SV::VAO::DeleteAttributeArray, "Cannot delete a nullptr buffer");

		this->Bind();

		Ref<ArrayBuffer> buffer = buffers[aa];
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glVertexAttribPointer(aa, 1, (GLenum)buffer->GetPrimitiveType(), GL_FALSE, 0, nullptr);

		buffer.reset();
		buffers.Remove(aa);
	}
	//
	void VAO::AddIndexBuffer(const Ref<IndexBuffer> ivbo)
	{
		if (indexBuffer != nullptr)
		{
			SV_WARN_T(SV::VAO::AddIndexBuffer, "Adding an index buffer to an already occupied slot. The current index buffer will be deleted.");
			indexBuffer.reset();
		}

		this->Bind();
		ivbo->Bind();

		indexBuffer = ivbo;
		arrayLength = ivbo->GetCount();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	//
	void VAO::DeleteIndexBuffer()
	{
		SV_ASSERT_T(indexBuffer == nullptr, SV::VAO::DeleteIndexBuffer, "Cannot delete a nullptr index buffer.");

		this->Bind();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		indexBuffer.reset();
	}
	//
	//
}
