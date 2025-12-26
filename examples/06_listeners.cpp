#include <config/config.hpp>
#include <iostream>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    auto &store = config::get_store("listeners.json");

    std::cout << "=== 基础监听器 ===" << std::endl;
    {
        // 监听单个键
        auto conn_id = store.connect(
            "username", [](const nlohmann::json &value) { std::cout << "用户名变更: " << value << std::endl; });

        store.set("username", "张三"); // 触发回调
        store.set("username", "李四"); // 触发回调

        // 取消监听
        store.disconnect(conn_id);

        store.set("username", "王五"); // 不触发回调
        std::cout << "监听器已断开" << std::endl;
    }

    std::cout << "\n=== 路径监听 ===" << std::endl;
    {
        // 监听路径前缀
        auto conn_id = store.connect(
            "user/profile", [](const nlohmann::json &value) { std::cout << "用户资料变更: " << value << std::endl; });

        store.set("user/profile/name", "张三");   // 触发
        store.set("user/profile/age", 25);        // 触发
        store.set("user/settings/theme", "dark"); // 不触发

        store.disconnect(conn_id);
    }

    std::cout << "\n=== 多个监听器 ===" << std::endl;
    {
        auto id1 = store.connect(
            "counter", [](const nlohmann::json &value) { std::cout << "监听器1: counter = " << value << std::endl; });

        auto id2 = store.connect("counter", [](const nlohmann::json &value) {
            std::cout << "监听器2: counter * 2 = " << (value.get<int>() * 2) << std::endl;
        });

        store.set("counter", 10); // 触发两个监听器

        store.disconnect(id1);
        store.set("counter", 20); // 只触发监听器2

        store.disconnect(id2);
    }

    std::cout << "\n=== 实际应用场景 ===" << std::endl;
    {
        // 主题切换监听
        auto theme_id = store.connect("ui/theme", [](const nlohmann::json &value) {
            std::cout << "应用主题切换为: " << value << std::endl;
            // 实际应用中可以在这里更新UI
        });

        // 语言切换监听
        auto lang_id = store.connect("ui/language", [](const nlohmann::json &value) {
            std::cout << "界面语言切换为: " << value << std::endl;
            // 实际应用中可以在这里重新加载语言包
        });

        store.set("ui/theme", "dark");
        store.set("ui/language", "zh-CN");
        store.set("ui/theme", "light");

        store.disconnect(theme_id);
        store.disconnect(lang_id);
    }

    system("pause");
    return 0;
}
