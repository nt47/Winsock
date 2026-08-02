#include "encoding.h"
namespace utils {

	std::wstring utf8_to_utf16(const std::string& s) {
		if (s.empty()) return {};
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
		std::wstring result(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), result.data(), size_needed);
		return result;
	}

	std::string utf16_to_utf8(const std::wstring& w) {
		if (w.empty()) return {};
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		std::string result(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), result.data(), size_needed, nullptr, nullptr);
		return result;
	}

	std::wstring ansi_to_utf16(const std::string& s) {
		if (s.empty()) return {};
		int size_needed = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
		std::wstring result(size_needed, 0);
		MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), result.data(), size_needed);
		return result;
	}

	std::string utf16_to_ansi(const std::wstring& w) {
		if (w.empty()) return {};
		int size_needed = WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		std::string result(size_needed, 0);
		WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), result.data(), size_needed, nullptr, nullptr);
		return result;
	}

} // namespace encoding
