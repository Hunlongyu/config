#pragma once

#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

#include <config/detail/types.hpp>

namespace config::detail
{

class PathResolver
{
  public:
    static std::string get_program_name()
    {
        std::string name = "config_app";
#if defined(CONFIG_TEST_FORCE_FALLBACK_PROGNAME)
        // Fallback testing mode: skip platform-specific logic
#elif defined(_WIN32)
        char module_path[MAX_PATH]{};
        const DWORD r = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
        if (r > 0 && r < MAX_PATH)
        {
            const std::filesystem::path p(module_path);
            name = p.stem().string();
        }
#elif defined(__APPLE__)
        char buf[4096];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
        {
            std::filesystem::path p(buf);
            name = p.stem().string();
        }
#elif defined(__linux__)
        char buf[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0)
        {
            buf[len] = '\0';
            std::filesystem::path p(buf);
            name = p.stem().string();
        }
#endif
        return name;
    }

    static std::filesystem::path get_appdata_path()
    {
#if defined(CONFIG_TEST_FORCE_FALLBACK_APPDATA)
        return std::filesystem::current_path();
#else
        std::filesystem::path app_path;
#if defined(_WIN32)
        WCHAR buff[MAX_PATH]{};
        bool success = false;
        success      = SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buff));
        if (success)
        {
            app_path = std::filesystem::path(std::wstring(buff)) / get_program_name();
        }
        else
        {
            app_path = std::filesystem::current_path();
        }
#elif defined(__APPLE__)
        const char *home = std::getenv("HOME");
        if (home)
            app_path = std::filesystem::path(home) / "Library" / "Application Support" / get_program_name();
        else
            app_path = std::filesystem::current_path();
#else
        const char *xdg  = std::getenv("XDG_CONFIG_HOME");
        const char *home = std::getenv("HOME");
        if (xdg)
            app_path = std::filesystem::path(xdg) / get_program_name();
        else if (home)
            app_path = std::filesystem::path(home) / ".config" / get_program_name();
        else
            app_path = std::filesystem::current_path();
#endif
        return app_path;
#endif
    }

    static std::string resolve(const std::string &path, const Path type)
    {
        const std::filesystem::path p(path);
        std::string result;
        std::filesystem::path app_data;

        switch (type)
        {
        case Path::AppData:
            app_data = get_appdata_path();
            if (!std::filesystem::exists(app_data))
            {
                std::filesystem::create_directories(app_data);
            }
            result = (app_data / p).string();
            break;
        case Path::Absolute:
        case Path::Relative:
        default:
            result = std::filesystem::absolute(p).string();
            break;
        }
        return result;
    }
};

} // namespace config::detail
