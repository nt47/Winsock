#pragma once
#include <string>
#include <windows.h>
namespace utils
{
	std::wstring utf8_to_utf16(const std::string& s);
	std::string utf16_to_utf8(const std::wstring& w);
	std::wstring ansi_to_utf16(const std::string& s);
	std::string utf16_to_ansi(const std::wstring& w);

	inline std::wstring s2w(const std::string& s) {
		return utf8_to_utf16(s);
	}
	inline std::string w2s(const std::wstring& w) {
		return utf16_to_utf8(w);
	}
}

