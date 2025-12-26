#include <config/config.hpp>
#include <iostream>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    // 1. 创建配置存储
    auto &store = config::get_store("basic_config.json");

    // 2. 设置基本数据类型
    store.set("app/name", "MyApp");
    store.set("app/version", "1.0.0");
    store.set("server/port", 8080);
    store.set("server/timeout", 30.5);
    store.set("features/enabled", true);

    // 3. 使用 JSON Pointer 路径
    store.set("user/profile/name", "张三");
    store.set("user/profile/age", 25);
    store.set("user/settings/theme", "dark");

    // 4. 读取配置（带默认值）
    auto app_name = store.get<std::string>("app/name", "Unknown");
    auto port     = store.get<int>("server/port", 3000);
    auto theme    = store.get<std::string>("user/settings/theme", "light");

    std::cout << "App: " << app_name << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Theme: " << theme << std::endl;

    // 5. 检查键是否存在
    if (store.contains("user/profile/name"))
    {
        std::cout << "用户名: " << store.get<std::string>("user/profile/name") << std::endl;
    }

    // 6. 删除键
    store.remove("user/settings/theme");

    // 7. 获取配置文件路径
    std::cout << "配置文件位置: " << store.get_store_path() << std::endl;

    system("pause");
    return 0;
}
