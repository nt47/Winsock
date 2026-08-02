#pragma once
#include <iostream>
#include<string>
#include <Windows.h>

class IniFile {
public:
	IniFile(const std::wstring& path) : path_(path) {}

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