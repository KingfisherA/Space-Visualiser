#pragma once
//
#pragma region OpenGL Binding Inclusions
//
//GLEW Library; The OpenGL binding library
//
#define GLEW_STATIC
#include<glew.h>
//
//SOIL Library
//
#define SOIL
#include<SOIL.h>
//
//GLM Library
//
//GLM Options
#define GLM_FORCE_RADIANS //Use radians instead of degrees in glm
#define GLM_FORCE_INLINE //Force the glm functions to be inlined
//
#include  <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <gtx/transform.hpp>
#include <gtx/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include<gtx\string_cast.hpp>
//
namespace glm
{
	using uivec2 = vec<2, unsigned int, defaultp>;
	using uivec3 = vec<3, unsigned int, defaultp>;
	using uivec4 = vec<4, unsigned int, defaultp>;
}
//
#pragma endregion
//
namespace SV
{
	//Forward declarations
	class VAO;
	class iVBO;
	class ArrayBuffer;
	class IndexBuffer;
	class ShaderProgram;
	class FBO;
	class Texture2D;

	enum class GLType
	{
		Null = GL_NONE,
		HalfFloat = GL_HALF_FLOAT,
		Float = GL_FLOAT,
		Double = GL_DOUBLE,
		Short = GL_SHORT,
		UShort = GL_UNSIGNED_SHORT,
		Int24Bit,
		Int = GL_INT,
		UInt = GL_UNSIGNED_INT,
		Byte = GL_BYTE,
		UByte = GL_UNSIGNED_BYTE,
		Char = GL_BYTE,
		UChar = GL_UNSIGNED_BYTE
	};

	class OpenGLContext final abstract
	{
	public:
		//
		static constexpr unsigned short OPENGL_MAJOR_VERSION = 4;
		static constexpr unsigned short OPENGL_MINOR_VERSION = 6;
		//
		static void __stdcall GLErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
		static bool InitiliseOpenGL();
		//
		inline static void EnableAlphaBlending() { glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
		inline static void EnableAdditiveBlending() { glBlendFunc(GL_SRC_ALPHA, GL_ONE); }
		//
		inline static const VAO* GetCurrentVAO() { return currentVAO; }
		inline static const iVBO* GetCurrentVBO() { return currentVBO; }
		inline static const FBO* GetCurrentFBO() { return currentFBO; }
		inline static const ShaderProgram* GetCurrentShaderProgram() { return currentProgram; }
		//
		inline static unsigned char TypeToByteSize(GLType type)
		{
			switch (type)
			{
			case SV::GLType::Null:
				return 0;
				break;
			case SV::GLType::HalfFloat:
				return 2;
				break;
			case SV::GLType::Float:
				return 4;
				break;
			case SV::GLType::Double:
				return 8;
				break;
			case SV::GLType::Short:
				return 2;
				break;
			case SV::GLType::UShort:
				return 2;
				break;
			case SV::GLType::Int:
				return 4;
				break;
			case SV::GLType::UInt:
				return 4;
				break;
			case SV::GLType::Byte:
				return 1;
				break;
			case SV::GLType::UByte:
				return 1;
				break;
			default:
				return 0;
				break;
			}
		}
		//
	private:
		//
		friend class SV::VAO;
		friend class  SV::ArrayBuffer;
		friend class  SV::ShaderProgram;
		friend class  SV::FBO;
		friend class  SV::IndexBuffer;
		//
		static const VAO* currentVAO;
		static const iVBO* currentVBO;
		static const FBO* currentFBO;
		static const ShaderProgram* currentProgram;
	};
}
