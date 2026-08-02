#pragma once
#include <iostream>
#include<filesystem>
#include<fstream>
#include<string>
#include <Windows.h>

std::wstring GetModuleDirectoryW();

class IniFile {
public:
	IniFile(const std::wstring& path) : path_(GetModuleDirectoryW() + path) {

		std::filesystem::path working_path = GetModuleDirectoryW() + path;
		//std::cout << working_path << std::endl;
		if (!std::filesystem::exists(working_path)) {
			//std::cout << "The .ini file doesn't exist!" << std::endl;
			if (!working_path.parent_path().empty())
				std::filesystem::create_directories(working_path.parent_path());
			std::ofstream file(working_path);
			if (!file) {
				printf("The .ini file creation failed! \n");
				return;
			}
			file.close();

		}
	}

	std::wstring Read(const std::wstring& section, const std::wstring& key)
	{
		wchar_t buffer[512]{};
		GetPrivateProfileStringW(
			section.c_str(),
			key.c_str(),
			L"",
			buffer,
			512,
			path_.c_str()
		);
		return buffer;
	}

	void Write(const std::wstring& section,
		const std::wstring& key,
		const std::wstring& value)
	{
		WritePrivateProfileStringW(
			section.c_str(),
			key.c_str(),
			value.c_str(),
			path_.c_str()
		);
	}

private:
	std::wstring path_;
};