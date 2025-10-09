#include"SV-Engine/Precompiled Global.hpp"
#include"ShaderProgram.hpp"
#include"SV-Engine/Utilities/SVUtilities.hpp"
#include"SV-Engine/Utilities/SVMemory.hpp"
#include"SV-Engine/Utilities/Errors.hpp"
#include"SV-Engine/OpenGL/Texture.hpp"
#include<utility>
#include<filesystem>
#include<string>
#include<unordered_map>
#include<iostream>


namespace SV
{
	//
#pragma region IShader
	//
	IShader::~IShader()
	{
		glDeleteShader(shaderID);
		shaderID = 0;
	}
	//
	static bool removeContinue = false;
	//
	static void EvaluatePreprocessorDirectives(SV::Array<SV::String>& onceIncludeHeaders, SV::String& currentLine, SV::String& sourceCode, const SV::StringView& lastInclude,
		const ShaderKeyMap& insertionKeys)
	{
		if (currentLine.Find("#pragma endremove") != IndexUndefined)
		{
			removeContinue = false;
			return;
		}
		else if (currentLine.Find("#pragma remove") != IndexUndefined)
		{
			removeContinue = true;
			return;
		}
		if (removeContinue) return;

		size_t pIndex = currentLine.Find("#pragma");
		if (pIndex != IndexUndefined)
		{
			if (size_t insertIndex = currentLine.Find("insert", pIndex); insertIndex != IndexUndefined)
			{
				size_t parBeginIndex = currentLine.Find('(', insertIndex + 6);
				if (parBeginIndex == IndexUndefined)
				{
					SV_WARN_T(SV::EvaluatePreprocessorDirectives, "Please begin #include directive file path with < or \"");
					return;
				}
				size_t parEndIndex = currentLine.Find(')', insertIndex + 6);
				if (parEndIndex == IndexUndefined)
				{
					SV_WARN_T(SV::EvaluatePreprocessorDirectives, "Please begin #include directive file path with < or \"");
					return;
				}
				SV::String insertKey = currentLine.Substring(parBeginIndex + 1, parEndIndex - parBeginIndex - 1);
				if (insertionKeys.empty())
				{
					SV_WARN_T(SV::EvaluatePreprocessorDirectives, "Insertion keys map was empty, thus #pragma insert ");
					//sourceCode += currentLine.Substring(0, pIndex) + currentLine.Substring(parEndIndex, currentLine.GetLength() - parEndIndex) + newLineChar;
					currentLine.SpliceAssign(pIndex, (parEndIndex + 1) - pIndex);
					sourceCode += currentLine + '\n';
					return;
				}

				auto insertKVPair = insertionKeys.find(insertKey);
				if (insertKVPair == insertionKeys.end())
				{
					SV_WARN_T(SV::EvaluatePreprocessorDirectives, ("#pragma insert(" + insertKey + ") could not be evaluated, as key map contains no member '" + insertKey + '\'').GetStringArray());
					//sourceCode += currentLine.Substring(0, pIndex) + currentLine.Substring(parEndIndex, currentLine.GetLength() - parEndIndex) + newLineChar;
					currentLine.SpliceAssign(pIndex, (parEndIndex + 1) - pIndex);
					sourceCode += currentLine + '\n';
					return;
				}
				sourceCode += currentLine.Substring(0, pIndex) + insertKVPair->second + currentLine.Substring(parEndIndex + 1, currentLine.GetLength() - parEndIndex - 1) + '\n';
			}
		}

		size_t includeIndex = currentLine.Find("#include");
		if (includeIndex != IndexUndefined)
		{
			size_t beginIndex = currentLine.Find('<', includeIndex + 8);
			beginIndex = beginIndex == IndexUndefined ? currentLine.Find('\"', includeIndex + 8) : beginIndex;
			if (beginIndex == IndexUndefined)
			{
				SV_FATAL_T(SV::EvaluatePreprocessorDirectives, "Please begin #include directive file path with < or \"");
				return;
			}

			size_t endIndex = currentLine.Find('>', beginIndex + 1);
			endIndex = endIndex == IndexUndefined ? currentLine.Find('\"', beginIndex + 1) : endIndex;
			if (endIndex == IndexUndefined)
			{
				SV_FATAL_T(SV::EvaluatePreprocessorDirectives, "Please end #include directive file path with > or \"");
				return;
			}

			size_t commentIndex = currentLine.Find("//", 0);
			if (commentIndex < includeIndex || commentIndex < endIndex)
			{
				sourceCode += (currentLine + '\n');
				return;
			}

			SV::String includePathRaw = currentLine.Substring(beginIndex + 1, endIndex - beginIndex - 1);
			SV::String filePath = std::filesystem::absolute(std::filesystem::path(includePathRaw.GetStringArray())).string();
			if (filePath == lastInclude)
			{
				SV_FATAL_T(EvaluatePreprocessorDirectives, ("Infinite inclusion loop. File " + filePath + " includes itself.").GetStringArray());
				return;
			}
			if (filePath.GetLength() == 0)
			{
				SV_FATAL_T(SV::EvaluatePreprocessorDirectives, "No file was given when evaluating #include directive");
				return;
			}
			if (onceIncludeHeaders.Find(filePath) == IndexUndefined)
			{
				SV::String fileData = move(Utilities::ReadFile(filePath));
				pIndex = fileData.Find("#pragma");
				if (pIndex != IndexUndefined)
				{
					size_t onceIndex = fileData.Find("once", pIndex);
					if (onceIndex != IndexUndefined)
					{
						onceIncludeHeaders.Add(filePath);
						fileData.SpliceAssign(pIndex, (onceIndex + 4) - pIndex);
					}
				}
				if (fileData.Contains("#include") || pIndex != IndexUndefined)
				{
					SV::Array<String> fileDataLines = move(fileData.Split('\n'));
					fileData.Clear(false);

					for (String& line : fileDataLines)
					{
						EvaluatePreprocessorDirectives(onceIncludeHeaders, line, fileData, filePath, insertionKeys);
					}

					fileData.TrimToCapacity();
				}
				sourceCode += (currentLine.Substring(0, includeIndex) + fileData + currentLine.Substring(endIndex + 1, currentLine.GetLength()) + '\n');
				return;
			}
			else
			{
				sourceCode += currentLine.Substring(0, includeIndex) + currentLine.Substring(endIndex + 1, currentLine.GetLength()) + '\n';
				return;
			}
		}

		if (pIndex == IndexUndefined && includeIndex == IndexUndefined) sourceCode += currentLine + '\n';
	}
	//
	void IShader::Preprocess(SV::String& sourceCode, const ShaderKeyMap& insertionKeys)
	{
		SV::Array<SV::String> onceIncludeHeaders;

		SV::Array<String> lines = move(sourceCode.Split('\n'));
		sourceCode.Clear(false);

		for (String& currentLine : lines)
		{
			EvaluatePreprocessorDirectives(onceIncludeHeaders, currentLine, sourceCode, ""_view, insertionKeys);
		}
		removeContinue = false;
		sourceCode.SpliceAssign(sourceCode.GetLength() - 1, 1);

		sourceCode.TrimToCapacity();
	}
	//
	IShader::IShader(ShaderType type, const SV::StringView& data, bool preprocess, const ShaderKeyMap& insertionKeys, bool isFilePath)
	{
		const char* rawData = nullptr;
		SV::String sourceCode;
		if (isFilePath && preprocess)
		{
			sourceCode = SV::Utilities::ReadFile(data.GetStringArray());
			IShader::Preprocess(sourceCode, insertionKeys);
			rawData = sourceCode.GetStringArray();
		}
		else if (isFilePath)
		{
			sourceCode = SV::Utilities::ReadFile(data.GetStringArray());
			rawData = sourceCode.GetStringArray();
		}
		else if (preprocess)
		{
			sourceCode = data;
			IShader::Preprocess(sourceCode, insertionKeys);
			rawData = sourceCode.GetStringArray();
		}
		else
		{
			rawData = data.GetStringArray();
		}


		shaderID = glCreateShader(GLenum(type));
		SV_ASSERT_T(shaderID == 0, SV::IShader::IShader, ("glCreateShader for " + [type]() -> SV::String
			{
				switch (type)
				{
				case SV::ShaderType::VertexShader:
					return "vertex shader";
					break;
				case SV::ShaderType::FragmentShader:
					return "fragment shader";
					break;
				case SV::ShaderType::GeometryShader:
					return "geometry shader";
					break;
				}
				return "unknown shader";
			}() + " returned 0. Check OpenGL context and try again.").GetStringArray());
		glShaderSource(shaderID, 1, &rawData, nullptr);
		glCompileShader(shaderID);

		int success = GL_FALSE;
		char* infoLog = nullptr;
		int infoLogSize = 0;
		glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);

		if (success == GL_FALSE)
		{
			glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infoLogSize);
#pragma warning(suppress: 6387)
			infoLog = (char*)alloca(infoLogSize);
			glGetShaderInfoLog(shaderID, infoLogSize, nullptr, infoLog);

#pragma warning(suppress: 6387)
			SV_WARN_T(SV::IShader::IShader, ("Shader Compilation failed.\nFile path = \"" + data + "\" \nID = " + SV::NumberToString(shaderID) + "\nLog: \n" + infoLog).GetStringArray());

		}
	}
	//
	IShader::IShader(ShaderType type, GLuint id)
	{
		SV_ASSERT_T(id == 0, SV::IShader::IShader, "Cannot construct a shader object on an invalid index. Index is 0.");
		shaderID = id;

		int success = GL_FALSE;
		char* infoLog = nullptr;
		int infoLogSize = 0;
		glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);

		if (success == GL_FALSE)
		{
			glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infoLogSize);
#pragma warning(suppress : 6387)
			infoLog = (char*)alloca(infoLogSize);
			glGetShaderInfoLog(shaderID, infoLogSize, nullptr, infoLog);

#pragma warning(suppress : 6387)
			SV_WARN_T(SV::IShader::IShader, ("Shader Compilation failed. ID=" + SV::NumberToString(shaderID) + ". Log= " + infoLog).GetStringArray())
		}
	}
	//
#pragma endregion
	//
#pragma region Shader Program
	//
	constexpr size_t DEFAULT_MIN_BUCKETS = 8;
	//
	static inline bool UniformMapCompare(const SV::StringView* lhs, const SV::StringView* rhs)
	{
		return (*lhs) == (*rhs);
	}
	//
	ShaderProgram::ShaderProgram() noexcept : uniformLocations(DEFAULT_MIN_BUCKETS, std::hash<const SV::StringView*>(), &UniformMapCompare)
	{
		shaderProgramID = glCreateProgram();
	}
	//
	ShaderProgram::ShaderProgram(GLuint id) noexcept : uniformLocations(DEFAULT_MIN_BUCKETS, std::hash<const SV::StringView*>(), &UniformMapCompare)
	{
		shaderProgramID = id;
	}
	//
	ShaderProgram::ShaderProgram(const VertexShader& vertexShader, const FragmentShader& fragmentShader, const AttributeList& attributeBindings) noexcept :
		uniformLocations(DEFAULT_MIN_BUCKETS, std::hash<const SV::StringView*>(), &UniformMapCompare)
	{
		shaderProgramID = glCreateProgram();

		AttachShader(vertexShader);
		AttachShader(fragmentShader);
		BindAttributes(attributeBindings);
		LinkShaderProgram();
	}
	//
	ShaderProgram::ShaderProgram(const std::initializer_list<const VertexShader>& vertexShaders, const std::initializer_list<const FragmentShader>& fragmentShaders, const AttributeList& attributeBindings) noexcept :
		uniformLocations(DEFAULT_MIN_BUCKETS, std::hash<const SV::StringView*>(), &UniformMapCompare)
	{
		shaderProgramID = glCreateProgram();

		AttachShaders(vertexShaders);
		AttachShaders(fragmentShaders);
		BindAttributes(attributeBindings);
		LinkShaderProgram();
	}
	//
	ShaderProgram::ShaderProgram(const VertexShader& vertexShader, const FragmentShader& fragmentShader, const GeometryShader& geometryShader, const AttributeList& attributeBindings) noexcept :
		uniformLocations(DEFAULT_MIN_BUCKETS, std::hash<const SV::StringView*>(), &UniformMapCompare)
	{
		shaderProgramID = glCreateProgram();

		AttachShader(vertexShader);
		AttachShader(fragmentShader);
		AttachShader(geometryShader);
		BindAttributes(attributeBindings);
		LinkShaderProgram();
	}
	//
	ShaderProgram::ShaderProgram(const std::initializer_list<const VertexShader>& vertexShaders, const std::initializer_list<const FragmentShader>& fragmentShaders,
		const std::initializer_list<const GeometryShader>& geometryShaders, const AttributeList& attributeBindings) noexcept :
		uniformLocations(DEFAULT_MIN_BUCKETS, std::hash<const SV::StringView*>(), &UniformMapCompare)
	{
		shaderProgramID = glCreateProgram();

		AttachShaders(vertexShaders);
		AttachShaders(fragmentShaders);
		AttachShaders(geometryShaders);
		BindAttributes(attributeBindings);
		LinkShaderProgram();
	}
	//
	ShaderProgram::~ShaderProgram() noexcept
	{
		glDeleteProgram(shaderProgramID);
		shaderProgramID = 0;

		this->ClearUniformCache();
	}
	//
	void ShaderProgram::AttachShaders(const std::initializer_list<const VertexShader>& shadersIn) noexcept
	{
		for (size_t i = 0; i < shadersIn.size(); i++)
		{
			SV_ASSERT_T(shadersIn.begin()[i].GetShader() == 0, SV::ShaderProgram::AttachShaders, "Shader cannot be nullptr");

			glAttachShader(shaderProgramID, shadersIn.begin()[i].GetShader());
			shaders.Add(&shadersIn.begin()[i]);
		}
	}
	//
	void ShaderProgram::AttachShaders(const std::initializer_list<const FragmentShader>& shadersIn) noexcept
	{
		for (size_t i = 0; i < shadersIn.size(); i++)
		{
			SV_ASSERT_T(shadersIn.begin()[i].GetShader() == 0, SV::ShaderProgram::AttachShaders, "Shader cannot be nullptr");

			glAttachShader(shaderProgramID, shadersIn.begin()[i].GetShader());
			shaders.Add(&shadersIn.begin()[i]);
		}
	}
	//
	void ShaderProgram::AttachShaders(const std::initializer_list<const GeometryShader>& shadersIn) noexcept
	{
		for (size_t i = 0; i < shadersIn.size(); i++)
		{
			SV_ASSERT_T(shadersIn.begin()[i].GetShader() == 0, SV::ShaderProgram::AttachShaders, "Shader cannot be nullptr");

			glAttachShader(shaderProgramID, shadersIn.begin()[i].GetShader());
			shaders.Add(&shadersIn.begin()[i]);
		}
	}
	//
	void ShaderProgram::AttachShader(const IShader& shader) noexcept
	{
		SV_ASSERT_T(shader.GetShader() == 0, SV::ShaderProgram::AttachShader, "Shader cannot be nullptr");

		glAttachShader(shaderProgramID, shader.GetShader());
		shaders.Add(&shader);
	}
	//
	void ShaderProgram::BindAttributes(const AttributeList& attributeBindings) noexcept
	{
		if (attributeBindings.size() == 0) return;
		for (size_t i = 0; i < attributeBindings.size(); i++)
		{
			const std::pair<AttributeArray, const char*>& attrib = attributeBindings.begin()[i];
			glBindAttribLocation(shaderProgramID, attrib.first, attrib.second);
		}
	}
	//
	void ShaderProgram::BindAttribute(AttributeArray aa, const char* name) noexcept { glBindAttribLocation(shaderProgramID, aa, name); }
	//
	void ShaderProgram::LinkShaderProgram() noexcept
	{
		glLinkProgram(shaderProgramID);

		for (const IShader* s : shaders)
		{
			glDetachShader(shaderProgramID, s->GetShader());
		}

		int success = 0;
		glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &success);
		if (success == GL_FALSE)
		{
			int logLength;
			glGetProgramiv(shaderProgramID, GL_INFO_LOG_LENGTH, &logLength);
			char* infoLog = (char*)alloca(logLength);
			glGetProgramInfoLog(shaderProgramID, logLength, NULL, infoLog);

			SV_WARN_T(SV::ShaderProgram::LinkShaderProgram, ("Shader Program '" + SV::NumberToString(shaderProgramID) + "' unable to link.\nLog: \n" + infoLog).GetStringArray());
		}

		shaders.Clear();
	}
	//
	void ShaderProgram::ClearUniformCache() noexcept
	{
		for (std::pair<const SV::StringView* const, Uniform>& uLocation : uniformLocations)
		{
			delete (SV::String*)uLocation.first;
		}
		uniformLocations.clear();
	}
	//
	Uniform ShaderProgram::GetUniformLocation(const SV::StringView& name) const noexcept(false)
	{
		if (auto kvPair = uniformLocations.find(&name); kvPair != uniformLocations.end())
		{
			return kvPair->second;
		}
		else
		{
			Uniform queryResult = glGetUniformLocation(shaderProgramID, (const GLchar*)name);

#ifdef SHADER_PROGRAM_DEBUG
			if (queryResult < 0)
			{
				puts("\n");
				SV_WARN_T(SV::ShaderProgram::GetUniformLocation, (const char*)("Uniform Location for \"" + name + "\" could not be found. Check spelling of the variable name and try again. Note that if the uniform variable is unused, OpenGL will elimate it."));
			}
#endif // DEBUG

			SV::String* newString = new(std::nothrow) SV::String(name);

			if (newString == nullptr)
			{
				SV_FATAL_T(SV::ShaderProgram::GetUniformLocation, "Could not allocate memory for uniform location cache.");
				throw std::runtime_error("Could not allocate memory for uniform location cache.");
			}

			uniformLocations.insert({ newString, queryResult });

			return queryResult;
		}
	}
	//
	void ShaderProgram::AddUniformLocations(UniformArrayView& keys) noexcept
	{
		for (auto& keyPair : keys)
		{
			uniformLocations.insert({ new SV::String(keyPair.first), keyPair.second });
		}
	}
	//
	void ShaderProgram::UploadTexture(const SV::StringView& name, const TextureUnits unit, const SV::ITexture& texture) const noexcept
	{
		texture.Bind(unit);
		this->UploadInt(name, (const int)unit);
	}
	//
	void ShaderProgram::UploadTexture(Uniform loc, const TextureUnits unit, const SV::ITexture& texture) noexcept
	{
		texture.Bind(unit);
		ShaderProgram::UploadInt(loc, (const int)unit);
	}
	//
	void ShaderProgram::UploadOptionalTexture(const SV::StringView& name, const TextureUnits unit, const SV::ITexture* texture) const noexcept
	{
		if (texture != nullptr)
		{
			texture->Bind(unit);
			this->UploadBool(name + ".hasTexture", true);
			this->UploadInt(name + ".texSampler", unit);
		}
		else
		{
			this->UploadBool(name + ".hasTexture", false);
		}
	}
	//
#pragma endregion
//
#pragma region Diffuse Shader
//
	const StringView DIFFUSE_VERTEX = "./Shaders/Miscellaneous/ColourDiffuse.v.glsl";
	const StringView DIFFUSE_FRAGMENT = "./Shaders/Miscellaneous/ColourDiffuse.f.glsl";
	DiffuseShader::DiffuseShader() : ShaderProgram(VertexShader(DIFFUSE_VERTEX), FragmentShader(DIFFUSE_FRAGMENT), AttributeList{ {0, "position"}, {1, "normal" } })
	{
		viewMatUni = GetUniformLocation("viewMatrix");
		projMatUni = GetUniformLocation("projectionMatrix");
		transformMatUni = GetUniformLocation("transformMatrix");
		normalMatUni = GetUniformLocation("normalMatrix");
		//
		lightPosUni = GetUniformLocation("lightPosition");
		lightColUni = GetUniformLocation("lightColour");
		//
		diffuseColourUni = GetUniformLocation("diffuseColour");
	}
	//
#pragma endregion
//
#pragma region Textured Diffuse Program
//
	const StringView TEXTURED_DIFFUSE_VERTEX = "./Shaders/Miscellaneous/TexturedDiffuse.v.glsl";
	const StringView TEXTURED_DIFFUSE_FRAGMENT = "./Shaders/Miscellaneous/TexturedDiffuse.f.glsl";
	TexturedDiffuseShader::TexturedDiffuseShader() : ShaderProgram(VertexShader(TEXTURED_DIFFUSE_VERTEX), FragmentShader(TEXTURED_DIFFUSE_FRAGMENT),
		AttributeList{ {0, "position"}, {1, "normal"}, {2, "texCoord"} })
	{
		viewMatUni = GetUniformLocation("viewMatrix");
		projMatUni = GetUniformLocation("projectionMatrix");
		transformMatUni = GetUniformLocation("transformMatrix");
		normalMatUni = GetUniformLocation("normalMatrix");
		//
		lightPosUni = GetUniformLocation("lightPosition");
		lightColUni = GetUniformLocation("lightColour");
		//
		diffuseMapUni = GetUniformLocation("diffuseMap");
	}
	//
#pragma endregion
	//
#pragma region Grid Shader
	//
	const StringView GRID_VERTEX = "./Shaders/Miscellaneous/GridShader.v.glsl";
	const StringView GRID_FRAGMENT = "./Shaders/Miscellaneous/GridShader.f.glsl";
	GridShader::GridShader() : ShaderProgram(VertexShader(GRID_VERTEX), 
		FragmentShader(GRID_FRAGMENT),
		AttributeList{ {0, "position"} })
	{
		vpMatUni = GetUniformLocation("vpMatrix");
		transformMatUni = GetUniformLocation("transformMatrix");
		camPosUni = GetUniformLocation("camPosition");
	}
	//
#pragma endregion
	//
}