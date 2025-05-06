#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool is_directory(const std::string &path)
{
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    return (attrib & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return S_ISDIR(info.st_mode);
#endif
}

bool is_path_creatable(const std::string &path)
{
#ifdef _WIN32
    // 检查父目录是否存在（Windows）
    size_t last_slash = path.find_last_of("\\/");
    if (last_slash == std::string::npos)
    {
        return true; // 当前目录可写（如 "file.txt"）
    }
    std::string parent_dir = path.substr(0, last_slash);
    DWORD attrib = GetFileAttributesA(parent_dir.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    // 检查父目录是否存在（Linux/macOS）
    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos)
    {
        return true; // 当前目录可写（如 "file.txt"）
    }
    std::string parent_dir = path.substr(0, last_slash);
    struct stat info;
    return (stat(parent_dir.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
#endif
}

static bool create_dir_if_not_exists(const std::string &path)
{
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0 || errno == EEXIST)
    {
        return true;
    }
#else
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST)
    {
        return true;
    }
#endif
    return false;
}