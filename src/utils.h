#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace Utils {

// Convert UTF-8 std::string to std::wstring
std::wstring ToWide(const std::string& str);

// Convert std::wstring to UTF-8 std::string
std::string ToUtf8(const std::wstring& wstr);

// Check if a file exists
bool FileExists(const std::string& path);

// Check if a directory exists
bool DirExists(const std::string& path);

// Try to auto-detect DDO Builder folder in common locations
std::string AutoDetectDDOBuilderFolder();

// Get timestamp string for commit messages (YYYY-MM-DD HH:MM:SS)
std::string GetTimestamp();

} // namespace Utils
