#include <config/config.hpp>
#include <filesystem>
#include <format>
#include <iostream>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "=== 相对路径（默认） ===" << std::endl;
    {
        auto &store = config::get_store("relative_config.json", config::Path::Relative);
        store.set("path_type", "relative");

        std::cout << std::format("配置文件路径: {}\n", store.get_store_path());
        std::cout << "相对于当前工作目录" << std::endl;
    }

    std::cout << "\n=== 绝对路径 ===" << std::endl;
    {
#ifdef _WIN32
        auto &store = config::get_store("C:/temp/absolute_config.json", config::Path::Absolute);
#else
        auto &store = config::get_store("/tmp/absolute_config.json", config::Path::Absolute);
#endif

        store.set("path_type", "absolute");

        std::cout << std::format("配置文件路径: {}\n", store.get_store_path());
        std::cout << "使用绝对路径" << std::endl;
    }

    std::cout << "\n=== AppData 路径 ===" << std::endl;
    {
        auto &store = config::get_store("MyApp/config.json", config::Path::AppData);
        store.set("path_type", "appdata");
        store.set("app_name", "MyApp");

        std::cout << std::format("配置文件路径: {}\n", store.get_store_path());
        std::cout << "位于系统 AppData 目录 (自动包含程序名以隔离配置)" << std::endl;

#ifdef _WIN32
        std::cout << "Windows: %LOCALAPPDATA%/<ExeName>/MyApp/config.json" << std::endl;
#elif __APPLE__
        std::cout << "macOS: ~/Library/Application Support/<ExeName>/MyApp/config.json" << std::endl;
#else
        std::cout << "Linux: ~/.config/<ExeName>/MyApp/config.json" << std::endl;
#endif
    }

    std::cout << "\n=== 多配置文件管理 ===" << std::endl;
    {
        // 用户配置（AppData）
        auto &user_cfg = config::get_store("MyApp/user.json", config::Path::AppData);
        user_cfg.set("username", "张三");
        user_cfg.set("theme", "dark");

        // 应用配置（相对路径）
        auto &app_cfg = config::get_store("app_config.json", config::Path::Relative);
        app_cfg.set("version", "1.0.0");
        app_cfg.set("debug", false);

// 系统配置（绝对路径）
#ifdef _WIN32
        auto &sys_cfg = config::get_store("C:/ProgramData/MyApp/system.json", config::Path::Absolute);
#else
        auto &sys_cfg = config::get_store("/etc/myapp/system.json", config::Path::Absolute);
#endif
        sys_cfg.set("system_wide", true);

        std::cout << std::format("用户配置: {}\n", user_cfg.get_store_path());
        std::cout << std::format("应用配置: {}\n", app_cfg.get_store_path());
        std::cout << std::format("系统配置: {}\n", sys_cfg.get_store_path());
    }

    system("pause");
    return 0;
}
