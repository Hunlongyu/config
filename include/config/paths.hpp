#pragma once

#include <config/detail/platform.hpp>
#include <config/error.hpp>
#include <cstdlib>
#include <filesystem>

namespace config
{
inline std::filesystem::path user_config_directory(const std::filesystem::path &application)
{
    if (application.empty() || application.has_parent_path() || application == "." || application == "..")
        throw Error(ErrorCode::path, "Application must be a single directory name");
#if defined(_WIN32)
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
        throw Error(ErrorCode::path, "Cannot locate LocalAppData");
    return std::filesystem::path(path) / application;
#else
    const char *home = std::getenv("HOME");
#if defined(__APPLE__)
    if (home && *home)
        return std::filesystem::path(home) / "Library" / "Application Support" / application;
#else
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg && std::filesystem::path(xdg).is_absolute())
        return std::filesystem::path(xdg) / application;
    if (home && *home)
        return std::filesystem::path(home) / ".config" / application;
#endif
    throw Error(ErrorCode::path, "Cannot locate user configuration directory");
#endif
}
} // namespace config
