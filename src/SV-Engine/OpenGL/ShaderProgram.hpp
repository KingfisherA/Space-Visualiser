#pragma once
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"SV-Engine/Utilities/String.hpp"
#include"SV-Engine/OpenGL/VAO.hpp"
#include"SV-Engine/Utilities/Maths.hpp"
#include"SV-Engine/Utilities/Errors.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include<unordered_map>
#include<functional>

typedef int Uniform;
typedef unsigned int TextureUnits;

#ifdef SV_DEBUG
#define SHADER_PROGRAM_DEBUG
#endif // SV_DEBUG


namespace SV
{
	//Forward Declarations
	class ITexture;

	//
	enum class ShaderType
	{
		VertexShader = GL_VERTEX_SHADER,
		FragmentShader = GL_FRAGMENT_SHADER,
		GeometryShader = GL_GEOMETRY_SHADER
	};

	using ShaderKeyMap = std::unordered_map<SV::String, SV::String>;
	//
	class IShader
	{
	public:
		IShader(const IShader& shaderIn)
		{
			shaderID = shaderIn.shaderID;
		}
		virtual ~IShader();
		//
		static void Preprocess(String& sourceCode, const ShaderKeyMap& insertionKeys = {});
		//
		inline GLuint GetShader() const { return shaderID; }
		virtual ShaderType GetShaderType() const = 0;
	protected:
		//
		IShader() = delete;
		//
		IShader(ShaderType type, const SV::StringView& data, bool preprocess = true, const ShaderKeyMap& insertionKeys = {}, bool isFilePath = true);
		IShader(ShaderType type, GLuint id);
		//
		GLuint shaderID;
		//
	};
	class FragmentShader : public IShader
	{
	public:
		FragmentShader(const SV::StringView& data, bool preprocess = true, const ShaderKeyMap& insertionKeys = {}, bool isFilePath = true) : IShader(SV::ShaderType::FragmentShader, data, preprocess, insertionKeys, isFilePath) {}
		FragmentShader(GLuint id) : IShader(SV::ShaderType::FragmentShader, id) {}
		virtual SV::ShaderType GetShaderType() const override { return ShaderType::FragmentShader; }
	};
	class VertexShader : public IShader
	{
	public:
		VertexShader(const SV::StringView& data, bool preprocess = true, const ShaderKeyMap& insertionKeys = {}, bool isFilePath = true) : IShader(SV::ShaderType::VertexShader, data, preprocess, insertionKeys, isFilePath) {}
		VertexShader(GLuint id) : IShader(SV::ShaderType::VertexShader, id) {}
		virtual SV::ShaderType GetShaderType()  const override { return ShaderType::VertexShader; }
	};
	class GeometryShader : public IShader
	{
	public:
		GeometryShader(const SV::StringView& data, bool preprocess = true, const ShaderKeyMap& insertionKeys = {}, bool isFilePath = true) : IShader(SV::ShaderType::GeometryShader, data, preprocess, insertionKeys, isFilePath) {}
		GeometryShader(GLuint id) : IShader(SV::ShaderType::GeometryShader, id) {}
		virtual SV::ShaderType GetShaderType()  const override { return ShaderType::GeometryShader; }
	};

	using AttributeList = std::initializer_list<std::pair<AttributeArray, const char*>>;
	using UniformMap = std::unordered_map<const SV::StringView*, Uniform, std::hash<const SV::StringView*>, std::function<bool(const SV::StringView*, const SV::StringView*)>>;
	using UniformArrayView = const ArrayView<std::pair<const SV::StringView, Uniform>>;

	//An OpenGL shader program object wrapper
	class ShaderProgram
	{
	public:
		//
		ShaderProgram() noexcept;
		ShaderProgram(const ShaderProgram&) = delete;
		ShaderProgram(ShaderProgram&&) = delete;
		ShaderProgram(GLuint id) noexcept;
		ShaderProgram(const VertexShader& vertexShader, const FragmentShader& fragmentShader, const AttributeList& attributeBindings = {}) noexcept;
		ShaderProgram(const std::initializer_list<const VertexShader>& vertexShaders, const std::initializer_list<const FragmentShader>& fragmentShaders, const AttributeList& attributeBindings = {}) noexcept;
		ShaderProgram(const VertexShader& vertexShader, const FragmentShader& fragmentShader, const GeometryShader& geometryShader, const AttributeList& attributeBindings = {}) noexcept;
		ShaderProgram(const std::initializer_list<const VertexShader>& vertexShaders, const std::initializer_list<const FragmentShader>& fragmentShaders, const std::initializer_list<const GeometryShader>& geometryShaders, 
			const AttributeList& attributeBindings = {}) noexcept;
		virtual ~ShaderProgram() noexcept;
		//
		void AttachShaders(const std::initializer_list<const VertexShader>& shadersIn) noexcept;
		void AttachShaders(const std::initializer_list<const FragmentShader>& shadersIn) noexcept;
		void AttachShaders(const std::initializer_list<const GeometryShader>& shadersIn) noexcept;
		void AttachShader(const IShader& shader) noexcept;
		//
		void BindAttributes(const AttributeList& attributeBindings) noexcept;
		void BindAttribute(AttributeArray aa, const char* name) noexcept;
		void LinkShaderProgram() noexcept;
		//
		void ForceBind() const noexcept
		{
			glUseProgram(shaderProgramID);
			OpenGLContext::currentProgram = this;
		}
		void Bind() const noexcept
		{
			if (OpenGLContext::currentProgram != this)
			{
				glUseProgram(shaderProgramID);
				OpenGLContext::currentProgram = this;
			}
		}
		static inline void Unbind() noexcept
		{
			glUseProgram(0);
			OpenGLContext::currentProgram = nullptr;
		}
		//
#pragma region Uniform Functions
		//
		void ClearUniformCache() noexcept;
		const UniformMap& GetUniformCache() const noexcept { return uniformLocations; }
		Uniform GetUniformLocation(const SV::StringView& name) const noexcept(false);
		void AddUniformLocations(UniformArrayView& keys) noexcept;
		//
		static inline void UploadInt(Uniform loc, const int val)  noexcept { glUniform1i(loc, val); }
		inline void UploadInt(const SV::StringView& name, const int val) const noexcept { glUniform1i(GetUniformLocation(name), val); }
		static inline void UploadInt(Uniform loc, const glm::ivec2& val) noexcept { glUniform2i(loc, val.x, val.y); }
		inline void UploadInt(const SV::StringView& name, const glm::ivec2& val) const  noexcept { glUniform2i(GetUniformLocation(name), val.x, val.y); }
		static inline void UploadInt(Uniform loc, const glm::ivec3& val)  noexcept { glUniform3i(loc, val.x, val.y, val.z); }
		inline void UploadInt(const SV::StringView& name, const glm::ivec3& val) const  noexcept { glUniform3i(GetUniformLocation(name), val.x, val.y, val.z); }
		static inline void UploadInt(Uniform loc, const glm::ivec4& val)  noexcept { glUniform4i(loc, val.x, val.y, val.z, val.w); }
		inline void UploadInt(const SV::StringView& name, const glm::ivec4& val) const  noexcept { glUniform4i(GetUniformLocation(name), val.x, val.y, val.z, val.w); }
		//
		static inline void UploadBool(Uniform loc, const bool val)  noexcept { glUniform1i(loc, (const int)val); }
		inline void UploadBool(const SV::StringView& name, const bool val) const  noexcept { glUniform1i(GetUniformLocation(name), (const int)val); }
		static inline void UploadBool(Uniform loc, const glm::bvec2& val)  noexcept { glUniform2i(loc, (const int)val.x, (const int)val.y); }
		inline void UploadBool(const SV::StringView& name, const glm::bvec2& val)const  noexcept { glUniform2i(GetUniformLocation(name), (const int)val.x, (const int)val.y); }
		static inline void UploadBool(Uniform loc, const glm::bvec3& val)  noexcept { glUniform3i(loc, (const int)val.x, (const int)val.y, (const int)val.z); }
		inline void UploadBool(const SV::StringView& name, const glm::bvec3& val) const  noexcept { glUniform3i(GetUniformLocation(name), (const int)val.x, (const int)val.y, (const int)val.z); }
		static inline void UploadBool(Uniform loc, const glm::bvec4& val)  noexcept { glUniform4i(loc, (const int)val.x, (const int)val.y, (const int)val.z, (const int)val.w); }
		inline void UploadBool(const SV::StringView& name, const glm::bvec4& val)const  noexcept { glUniform4i(GetUniformLocation(name), (const int)val.x, (const int)val.y, (const int)val.z, (const int)val.w); }
		//
		static inline void UploadUInt(Uniform loc, const unsigned int val)  noexcept { glUniform1ui(loc, val); }
		inline void UploadUInt(const SV::StringView& name, const unsigned int val) const  noexcept { glUniform1ui(GetUniformLocation(name), val); }
		static inline void UploadUInt(Uniform loc, const glm::uivec2& val)  noexcept { glUniform2ui(loc, val.x, val.y); }
		inline void UploadUInt(const SV::StringView& name, const glm::uivec2& val) const  noexcept { glUniform2ui(GetUniformLocation(name), val.x, val.y); }
		static inline void UploadUInt(Uniform loc, const glm::uivec3& val)  noexcept { glUniform3ui(loc, val.x, val.y, val.z); }
		inline void UploadUInt(const SV::StringView& name, const glm::uivec3& val) const  noexcept { glUniform3ui(GetUniformLocation(name), val.x, val.y, val.z); }
		static inline void UploadUInt(Uniform loc, const glm::uivec4& val)  noexcept { glUniform4ui(loc, val.x, val.y, val.z, val.w); }
		inline void UploadUInt(const SV::StringView& name, const glm::uivec4& val) const  noexcept { glUniform4ui(GetUniformLocation(name), val.x, val.y, val.z, val.w); }
		//
		static inline void UploadFloat(Uniform loc, const float val)  noexcept { glUniform1f(loc, val); }
		inline void UploadFloat(const SV::StringView& name, const float val) const  noexcept { glUniform1f(GetUniformLocation(name), val); }
		static inline void UploadFloat(Uniform loc, const glm::fvec2& val)  noexcept { glUniform2f(loc, val.x, val.y); }
		inline void UploadFloat(const SV::StringView& name, const glm::fvec2& val) const  noexcept { glUniform2f(GetUniformLocation(name), val.x, val.y); }
		static inline void UploadFloat(Uniform loc, const glm::fvec3& val)  noexcept { glUniform3f(loc, val.x, val.y, val.z); }
		inline void UploadFloat(const SV::StringView& name, const glm::fvec3& val)const  noexcept { glUniform3f(GetUniformLocation(name), val.x, val.y, val.z); }
		static inline void UploadFloat(Uniform loc, const glm::fvec4& val)  noexcept { glUniform4f(loc, val.x, val.y, val.z, val.w); }
		inline void UploadFloat(const SV::StringView& name, const glm::fvec4& val) const  noexcept { glUniform4f(GetUniformLocation(name), val.x, val.y, val.z, val.w); }
		//
		static inline void UploadDouble(Uniform loc, const double val)  noexcept { glUniform1d(loc, val); }
		inline void UploadDouble(const SV::StringView& name, const double val) const  noexcept { glUniform1d(GetUniformLocation(name), val); }
		static inline void UploadDouble(Uniform loc, const glm::dvec2& val)  noexcept { glUniform2d(loc, val.x, val.y); }
		inline void UploadDouble(const SV::StringView& name, const glm::dvec2& val) const  noexcept { glUniform2d(GetUniformLocation(name), val.x, val.y); }
		static inline void UploadDouble(Uniform loc, const glm::dvec3& val) noexcept { glUniform3d(loc, val.x, val.y, val.z); }
		inline void UploadDouble(const SV::StringView& name, const glm::dvec3& val) const  noexcept { glUniform3d(GetUniformLocation(name), val.x, val.y, val.z); }
		static inline void UploadDouble(Uniform loc, const glm::dvec4& val)  noexcept { glUniform4d(loc, val.x, val.y, val.z, val.w); }
		inline void UploadDouble(const SV::StringView& name, const glm::dvec4& val) const  noexcept { glUniform4d(GetUniformLocation(name), val.x, val.y, val.z, val.w); }
		//
		static inline void UploadMatrixF(Uniform loc, const glm::fmat4& v)  noexcept { glUniformMatrix4fv(loc, 1, GL_FALSE, &v[0][0]); }
		inline void UploadMatrixF(const SV::StringView& name, const glm::fmat4& v) const  noexcept { glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &v[0][0]); }
		static inline void UploadMatrixF(Uniform loc, const glm::fmat3& v)  noexcept { glUniformMatrix3fv(loc, 1, GL_FALSE, &v[0][0]); }
		inline void UploadMatrixF(const SV::StringView& name, const glm::fmat3& v) const  noexcept { glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, &v[0][0]); }
		//
		static inline void UploadMatrixD(Uniform loc, const glm::dmat4& v)  noexcept { glUniformMatrix4dv(loc, 1, GL_FALSE, &v[0][0]); }
		inline void UploadMatrixD(const SV::StringView& name, const glm::dmat4& v) const  noexcept { glUniformMatrix4dv(GetUniformLocation(name), 1, GL_FALSE, &v[0][0]); }
		static inline void UploadMatrixD(Uniform loc, const glm::dmat3& v)  noexcept { glUniformMatrix3dv(loc, 1, GL_FALSE, &v[0][0]); }
		inline void UploadMatrixD(const SV::StringView& name, const glm::dmat3& v) const  noexcept { glUniformMatrix3dv(GetUniformLocation(name), 1, GL_FALSE, &v[0][0]); }
		//
		void UploadTexture(const SV::StringView& name, const TextureUnits unit, const SV::ITexture& texture) const noexcept;
		static void UploadTexture(Uniform loc, const TextureUnits unit, const SV::ITexture& texture) noexcept;
		void UploadOptionalTexture(const SV::StringView& name, const TextureUnits unit, const SV::ITexture* texture) const noexcept;
		//
#pragma endregion
		//
		inline GLuint GetProgram() const noexcept { return shaderProgramID; }
		//
		explicit inline operator unsigned int() const noexcept { return shaderProgramID; }
		//
	private:
		//
		GLuint shaderProgramID = 0;
		//
		SV::Array<const SV::IShader*> shaders;
		//
		mutable UniformMap uniformLocations;
		//
	};

	//
	class DiffuseShader : public ShaderProgram
	{
	public:
		DiffuseShader();
		//
		inline void BindViewMat(const glm::mat4& value) { UploadMatrixF(viewMatUni, value); }
		inline void BindProjMat(const glm::mat4& value) { UploadMatrixF(projMatUni, value); }
		inline void BindTransMat(const glm::mat4& value) { UploadMatrixF(transformMatUni, value); BindNormalMat(Maths::CalculateNormalMatrix(value)); }
		inline void BindNormalMat(const glm::mat3& value) { UploadMatrixF(normalMatUni, value); }
		//
		inline void BindLightPos(const glm::vec3& value) { UploadFloat(lightPosUni, value); }
		inline void BindLightCol(const glm::vec3& value) { UploadFloat(lightColUni, value); }
		//
		inline void BindDiffuseColour(const glm::vec4& value) { UploadFloat(diffuseColourUni, value); }
		//
	private:
		//
		Uniform viewMatUni = 0;
		Uniform projMatUni = 0;
		Uniform transformMatUni = 0;
		Uniform normalMatUni = 0;
		//
		Uniform lightPosUni = 0;
		Uniform lightColUni = 0;
		//
		Uniform diffuseColourUni = 0;
	};

	//
	class TexturedDiffuseShader : public ShaderProgram
	{
	private:
		//
		Uniform viewMatUni = 0;
		Uniform projMatUni = 0;
		Uniform transformMatUni = 0;
		Uniform normalMatUni = 0;
		//
		Uniform lightPosUni = 0;
		Uniform lightColUni = 0;
		//
		Uniform diffuseMapUni = 0;
		//
	public:
		TexturedDiffuseShader();
		TexturedDiffuseShader(const TexturedDiffuseShader&) = default;
		TexturedDiffuseShader(TexturedDiffuseShader&&) = delete;
		//
		inline void BindViewMat(const glm::mat4& value) { UploadMatrixF(viewMatUni, value); }
		inline void BindProjMat(const glm::mat4& value) { UploadMatrixF(projMatUni, value); }
		inline void BindTransMat(const glm::mat4& value) { UploadMatrixF(transformMatUni, value); BindNormalMat(Maths::CalculateNormalMatrix(value)); }
		inline void BindNormalMat(const glm::mat3& value) { UploadMatrixF(normalMatUni, value); }
		//
		inline void BindLightPos(const glm::vec3& value) { UploadFloat(lightPosUni, value); }
		inline void BindLightCol(const glm::vec3& value) { UploadFloat(lightColUni, value); }
		//
		inline void BindDiffuseMapUnit(const uint value) { UploadInt(diffuseMapUni, (const int)value); }
		//
	};

	//
	class GridShader : public ShaderProgram
	{
	public:
		GridShader();
		//
		inline void BindVPMatrix(const glm::mat4& value) { UploadMatrixF(vpMatUni, value); }
		inline void BindTransMat(const glm::mat4& value) { UploadMatrixF(transformMatUni, value); }
		//
		inline void BindCamPos(const glm::vec3& value) { UploadFloat(camPosUni, value); }
		//
	private:
		//
		Uniform vpMatUni = 0;
		Uniform transformMatUni = 0;
		//
		Uniform camPosUni = 0;
	};
}