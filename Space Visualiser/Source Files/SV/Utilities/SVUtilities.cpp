#include"SV/Precompiled Global.hpp"
#include"SVUtilities.hpp"

#include"SV/Utilities/GlobalItems.hpp"
#include<vector>
#include<iostream>
#include<string>
#include<cstdlib>
#include"SV/Utilities/String.hpp"
#include"SV/Utilities/Errors.hpp"

namespace SV
{

#pragma region Utilities

	//
	SV::String Utilities::ReadFile(const SV::StringView filename)
	{
		std::ifstream inFile(filename.GetStringArray());
		if (inFile.fail())
		{
			SV_FATAL_T(SV::Utilities::ReadFile, (const char*)("Filename "_str + filename + " is invalid and could not be read"));
			throw std::exception((const char*)("Filename "_str + filename + " is invalid and could not be read"));
		}

		std::string stringIn((std::istreambuf_iterator<char>(inFile)), (std::istreambuf_iterator<char>()));

		inFile.close();

		return SV::String(std::move(stringIn));
	}
	//
	bool Utilities::IsExtensionCompatible(const StringView& extension)
	{
		StringView extensionStr = (const char*)glGetString(GL_EXTENSIONS);
		SV::Array<StringView> extensions = move(extensionStr.SplitView(' '));

		for (size_t i = 0; i < extensions.GetCount(); i++)
		{
			if (extensions[i] == extension) return true;
		}

		return false;
	}

#pragma endregion

}
