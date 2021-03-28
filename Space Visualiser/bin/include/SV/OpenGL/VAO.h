#pragma once
#include"SV/Utilities/GlobalItems.h"
#include"SV/Utilities/Array.h"
#include"SV/Utilities/SVMemory.h"
#include"VBO.h"
#include<initializer_list>
#include<vector>

typedef unsigned char AttributeArray;
template<typename A> class SV::Array;

namespace SV
{
	//
	class VAO
	{
	private:
		//
		SV::Array<Ref<ArrayBuffer>> buffers;
		Ref<IndexBuffer> indexBuffer;
		//
		unsigned int arrayLength = 0;
		GLenum renderMethod = GL_TRIANGLES;
		//
		GLuint vaoID = 0;
		//
	public:
		//
		static constexpr unsigned int MAX_ATTRIB_ARRAYS = 16;
		static Array<Ref<VAO>> GenerateArrays(unsigned int number);
		//
		VAO();
		VAO(VAO& vaoIn);
		VAO(GLuint id);
		//
		~VAO();
		//
		void ForceBind(bool index = true) const
		{
			glBindVertexArray(vaoID);
			if (index) BindIndexBuffer();
			OpenGLContext::currentVAO = const_cast<VAO*>(this);
		}
		void Bind(bool index = true) const
		{
			if (OpenGLContext::currentVAO != this)
			{
				glBindVertexArray(vaoID);
				OpenGLContext::currentVAO = const_cast<VAO*>(this);
			}
			if (index) BindIndexBuffer();
		}
		static void Unbind(bool index = true)
		{
			glBindVertexArray(0);
			OpenGLContext::currentVAO = nullptr;
			if (index) UnbindIndexBuffer();
		}
		//
		void BindIndexBuffer() const { if (indexBuffer != nullptr) indexBuffer->Bind(); }
		static inline void UnbindIndexBuffer() { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }
		//
		void AddAttributeArray(AttributeArray aa, const Ref<ArrayBuffer> buffer, const AttributeLayout& layout = AttributeLayout());
		void SetArrayInstanceDivisor(AttributeArray aa, unsigned int divisor);
		inline Ref<ArrayBuffer> GetAttributeArray(AttributeArray aa) const { return buffers[aa]; }
		void DeleteAttributeArray(AttributeArray aa);
		//
		void AddIndexBuffer(const Ref<IndexBuffer> ivbo);
		inline Ref<IndexBuffer> GetIndexBuffer() const { return indexBuffer; }
		void DeleteIndexBuffer();
		//
		static inline void EnableAttributeArray(AttributeArray aa) { glEnableVertexAttribArray(aa); }
		static void EnableAttributeArray(const std::initializer_list<AttributeArray>& aa)
		{
			for (unsigned int i = 0; i < aa.size(); i++)
			{
				glEnableVertexAttribArray(*(aa.begin() + i));
			}
		}
		static void EnableAttributeArray(AttributeArray begin, AttributeArray end)
		{
			if (end > MAX_ATTRIB_ARRAYS - 1) return;
			for (unsigned int i = begin; i <= end; i++)
			{
				glEnableVertexAttribArray(i);
			}
		}
		static inline void DisableAttributeArray(AttributeArray aa) { glDisableVertexAttribArray(aa); }
		static void DisableAttributeArray(const std::initializer_list<AttributeArray>& aa)
		{
			for (unsigned int i = 0; i < aa.size(); i++)
			{
				glDisableVertexAttribArray(*(aa.begin() + i));
			}
		}
		static void DisableAttributeArray(AttributeArray begin, AttributeArray end)
		{
			if (end > MAX_ATTRIB_ARRAYS - 1) return;
			for (unsigned int i = begin; i <= end; i++)
			{
				glDisableVertexAttribArray(i);
			}
		}
		//
		inline GLuint GetID() const { return vaoID; }
		//
		inline unsigned int GetEleCount() const { return arrayLength; }
		inline GLenum GetRenderMethod() const { return renderMethod; }
		//
	};
}
