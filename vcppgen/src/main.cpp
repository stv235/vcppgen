#include <iostream>
#include <vector>
#include <experimental/filesystem>
#include <fstream>
#include <string_view>

namespace fs = std::experimental::filesystem;

struct Configuration
{
	std::string name;
	std::string architecture;

	std::vector<std::string> libraries;
	std::vector<std::string> binaries;
};

struct Project
{
	std::string name;
	std::string toolset;

	bool hasBinaries = false;
	bool hasLibraries = false;

	std::vector<Configuration> configurations;
};

void writeCondition(std::ostream& os, const Configuration& configuration)
{
	os << "Condition=\"'$(Configuration)|$(Platform)'=='" << configuration.name << '|' << configuration.architecture << "'\"";
}

void writeCopyTarget(std::ofstream& os, const Project& project)
{
	if (project.hasBinaries)
	{
		os << "\t<Target Name=\"CopyBinaryFiles\">\r\n"
			<< "\t\t<ItemGroup>\r\n";	

		for (const auto configuration : project.configurations)
		{
			for (const auto binary : configuration.binaries)
			{
				os << "<NativeTargetPath ";
				writeCondition(os, configuration);
				os << " Include=\"$(ProjectDir)" << binary << "\" />\r\n";
			}			
		}

		os << "</ItemGroup>\r\n"
			<< "<Copy SourceFiles=\"@(NativeTargetPath)\" DestinationFolder=\"$(OutDir)\" />\r\n"
			<< "</Target>\r\n"
			<< "<Target Name=\"Build\" DependsOnTargets=\"CopyBinaryFiles\"/>";
	}
}

void writeTargets(std::ofstream& os, const Configuration& configuration)
{
	for (const auto binary : configuration.binaries)
	{
		os << "\t\t\t<NativeTargetPath Condition=\"'$(Configuration)|$(Platform)'=='"
			<< configuration.name
			<< '|' << configuration.architecture
			<< "' and '$(DesignTimeBuild)'=='true'\" Include=\"$(ProjectDir)"
			<< binary
			<< "\" />\r\n";
	}

	for (const auto library : configuration.libraries)
	{
		os << "\t\t\t<NativeTargetPath Condition=\"'$(Configuration)|$(Platform)'=='"
			<< configuration.name
			<< '|' << configuration.architecture
			<< "' and '$(DesignTimeBuild)'=='true'\" Include=\"$(ProjectDir)"
			<< library
			<< "\" />\r\n";
	}
}



void writeLibraries(std::ostream& os, const Configuration& configuration)
{
	os << "<Libs ";
	writeCondition(os, configuration);
	os << " Include=\"";

	for (auto it = configuration.libraries.begin(); it != configuration.libraries.end(); ++it)
	{
		if (it != configuration.libraries.begin())
		{
			os << ';';
		}
		
		os << "$(ProjectDir)" << *it;
	}

	os << "\">\r\n"
		"<ProjectType>";

	if (configuration.binaries.empty())
	{
		os << "StaticLibrary";
	}
	else
	{
		os << "DynamicLibrary";
	}

	os << "</ProjectType>\r\n"
		"<FileType>lib</FileType>\r\n"
		"<ResolveableAssembly>false</ResolveableAssembly>\r\n"
		"</Libs>\r\n";	
}

void writeLibraryTarget(std::ostream& os, const Project& project)
{
	if (project.hasLibraries)
	{
		os << "<Target Name=\"GetResolvedLinkLibs\"";

		os << " Returns = \"@(Libs)\">\r\n"
			"<ItemGroup>\r\n";

		for (const auto configuration : project.configurations)
		{
			writeLibraries(os, configuration);
		}

		os << "</ItemGroup>\r\n"
			"</Target>\r\n";
	}
	
}

void writeConfigurations(std::ostream& os, const Project& project)
{
	os << "<ItemGroup Label=\"ProjectConfigurations\">\r\n";

	for (const auto& configuration : project.configurations)
	{
		os << "<ProjectConfiguration Include=\""
			<< configuration.name << '|' << configuration.architecture << "\">\r\n"
			"<Configuration>" << configuration.name << "</Configuration>\r\n"
			"<Platform>" << configuration.architecture << "</Platform>\r\n"
			"</ProjectConfiguration>\r\n";


	}
	os << "</ItemGroup>\r\n"
		"<PropertyGroup Label=\"Configuration\">\r\n"
		"<PlatformToolset>" << project.toolset << "</PlatformToolset>\r\n"
		"</PropertyGroup>\r\n";
}

void writeGlobals(std::ostream& os, const Project& project)
{
	os << "<PropertyGroup Label=\"Globals\">\r\n"
//		<< "<VCProjectVersion>15.0</VCProjectVersion>\r\n"
		//<< "<ProjectGuid>{1711947A-37E6-48BD-B50F-D957825032A6}</ProjectGuid>
		<< "<Keyword>Win32Proj</Keyword>"
		<< "<RootNamespace>" << project.name << "</RootNamespace>\r\n"
		<< "</PropertyGroup>\r\n";
}

int main(int argc, char* argv[])
{
	try
	{
		const auto rootDir = fs::current_path();

		auto i = 1;
		const auto requireFn = [&](const std::string str)
		{
			if (i >= argc)
			{
				throw std::runtime_error(str);
			}

			std::string_view param = argv[i];

			++i;
			return param;
		};

		const auto requireSwitchFn = [&](const std::string str, const std::string val)
		{
			if (i >= argc)
			{
				throw std::runtime_error(str);
			}

			const std::string_view param = argv[i];

			if (param != val)
			{
				throw std::runtime_error(str);
			}

			++i;
		};

		Project project;

		project.name = requireFn("Expected project name");
		project.toolset = requireFn("Expected toolset (e.g. vs141, vs140)");

		while (i < argc)
		{
			Configuration configuration;

			requireSwitchFn("Expected -c", "-c");
			configuration.name = requireFn("Expected configurations name");
			configuration.architecture = requireFn("Expected platform (e.g. x86, x64)");

			while (i < argc)
			{
				const std::string_view sw = argv[i];

				if (sw == "-c")
				{
					break;
				}
				
				if (sw == "-dll")
				{
					++i;
					const auto binary = requireFn("Expected DLL path");

					if (!fs::is_regular_file(rootDir / std::string{ binary }))
					{
						throw std::runtime_error("DLL file not found: '" + std::string{ binary } +"'");
					}

					project.hasBinaries = true;
					configuration.binaries.emplace_back(binary);
				}
				else if (sw == "-lib")
				{
					++i;
					const auto library = requireFn("Expected lib path");

					if (!fs::is_regular_file(rootDir / std::string{ library }))
					{
						throw std::runtime_error("lib file not found: '" + std::string{ library } +"'");
					}

					project.hasLibraries = true;
					configuration.libraries.emplace_back(library);
				}
				else
				{
					throw std::runtime_error("Expected -dll, -lib or -c");
				}
			}

			if (configuration.libraries.empty())
			{
				std::cout << "Warning: Configuration '" << configuration.name << '|' << configuration.architecture << "' has no libs" << std::endl;
			}

			if (configuration.binaries.empty())
			{
				std::cout << "Warning: Configuration '" << configuration.name << '|' << configuration.architecture << "' has no DLLs" << std::endl;
			}

			project.configurations.push_back(configuration);
		}

		const auto outPath = rootDir / (project.name + ".vcxproj");

		//if (!fs::exists(outPath))
		{
			std::ofstream os{ outPath };


			os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
				"<Project DefaultTargets=\"Build\" ToolsVersion=\"15.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n";

			writeConfigurations(os, project);
			writeGlobals(os, project);

			// Default targets
			os << "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\"/>\r\n"
				"<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\"/>\r\n"
				"<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\"/>\r\n";

			os << "\t<Target Name=\"GetTargetPath\" DependsOnTargets=\"GetNativeTargetPath\" Returns=\"@(NativeTargetPath)\">\r\n"
				"\t</Target>\r\n";

			// Binary targets
			os << "\t<Target Name=\"GetTargetPath\" Returns=\"@(NativeTargetPath)\">\r\n"
				"\t\t<ItemGroup>\r\n";

			for (const auto configuration : project.configurations)
			{
				writeTargets(os, configuration);
			}

			os << "\t\t</ItemGroup>\r\n"
				"\t</Target>\r\n";

			writeLibraryTarget(os, project);
			writeCopyTarget(os, project);

			os << "</Project>\r\n";
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "Error: " << e.what() << std::endl
			<< std::endl
			<< "Usage: vcppgen <name> <toolset> -c <configuration> <platform> -dll <dll path>... - lib <lib - path>... - c ..." << std::endl
			<< "Example: vcppgen test vc141 -c Debug x64 -dll Debug\\test.dll -lib Debug\\test.lib -c Release x64 -dll Release\\test.dll -lib Release\\test.lib" << std::endl;
	}
}
