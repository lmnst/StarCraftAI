#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <stdexcept>

namespace StarBench {
inline std::wstring ProjectRoot() {
    wchar_t buffer[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, 32768);
    if (!length || length == 32768) throw std::runtime_error("Cannot locate the executable.");
    std::wstring directory(buffer, length);
    while (true) {
        const auto separator = directory.find_last_of(L"\\/");
        if (separator == std::wstring::npos) break;
        directory.resize(separator);
        const std::wstring marker = directory + L"\\tools\\RuntimePaths.h";
        const DWORD attributes = GetFileAttributesW(marker.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return directory + L"\\";
    }
    throw std::runtime_error("Keep the executable inside the StarCraftAI project directory.");
}
inline std::string Narrow(const std::wstring& value) {
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_ACP, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    WideCharToMultiByte(CP_ACP, 0, value.data(), (int)value.size(), &result[0], count, nullptr, nullptr);
    return result;
}
inline std::wstring Widen(const std::string& value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_ACP, 0, value.data(), (int)value.size(), nullptr, 0);
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_ACP, 0, value.data(), (int)value.size(), &result[0], count);
    return result;
}
struct EnvironmentOverride {
    std::wstring name, original;
    bool present;
    EnvironmentOverride(const wchar_t* key, const std::wstring& value): name(key) {
        DWORD size = GetEnvironmentVariableW(key, nullptr, 0);
        present = size != 0;
        if (present) { std::vector<wchar_t> buffer(size); GetEnvironmentVariableW(key, buffer.data(), size); original = buffer.data(); }
        if (!SetEnvironmentVariableW(key, value.c_str())) throw std::runtime_error("Cannot configure child process environment.");
    }
    ~EnvironmentOverride() { SetEnvironmentVariableW(name.c_str(), present ? original.c_str() : nullptr); }
};
}