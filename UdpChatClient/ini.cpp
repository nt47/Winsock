#include"ini.h"

std::filesystem::path GetModulePath(HMODULE module) {
	wchar_t buffer[MAX_PATH];
	DWORD len = GetModuleFileNameW(module, buffer, MAX_PATH);
	return std::filesystem::path(buffer, buffer + len);
}

extern "C" IMAGE_DOS_HEADER __ImageBase;

HMODULE GetCurrentModule() {
	return reinterpret_cast<HMODULE>(&__ImageBase);
}

std::filesystem::path GetModuleDirectoryWithoutSlash() {
	auto module_path = GetModulePath(GetCurrentModule());
	return module_path.parent_path();
}

std::wstring GetModuleDirectoryW()
{
	std::filesystem::path dir = GetModuleDirectoryWithoutSlash();
	dir /= "";

	return dir.wstring();
}

