#include <chrono>
#include <config/config.hpp>
#include <iostream>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "=== 自动保存模式 ===" << std::endl;
    {
        auto &store = config::get_store("auto_save.json", config::Path::Relative, config::SaveStrategy::Auto);

        store.set("key1", "value1"); // 自动保存
        store.set("key2", "value2"); // 自动保存
        store.set("key3", "value3"); // 自动保存

        std::cout << "每次 set 都会自动保存到文件" << std::endl;
    }

    std::cout << "\n=== 手动保存模式 ===" << std::endl;
    {
        auto &store = config::get_store("manual_save.json", config::Path::Relative, config::SaveStrategy::Manual);

        // 批量设置（不会保存）
        for (int i = 0; i < 100; ++i)
        {
            store.set("item_" + std::to_string(i), i);
        }
        std::cout << "批量设置了 100 个配置项（未保存）" << std::endl;

        // 使用默认格式保存
        store.save();
        std::cout << "手动调用 save() 保存" << std::endl;

        // 临时使用压缩格式保存
        store.set("temp", "data");
        store.save(config::JsonFormat::Compact);
        std::cout << "使用压缩格式保存" << std::endl;
    }

    std::cout << "\n=== 动态切换策略 ===" << std::endl;
    {
        auto &store = config::get_store("dynamic_save.json");

        // 初始为自动保存
        store.set("auto_1", "value"); // 立即保存

        // 切换到手动保存
        store.set_save_strategy(config::SaveStrategy::Manual);
        store.set("manual_1", "value"); // 不保存
        store.set("manual_2", "value"); // 不保存
        store.save();                   // 手动保存

        // 切换回自动保存
        store.set_save_strategy(config::SaveStrategy::Auto);
        store.set("auto_2", "value"); // 立即保存

        std::cout << "策略切换完成" << std::endl;
    }

    std::cout << "\n=== 格式设置 ===" << std::endl;
    {
        auto &store = config::get_store("format_test.json");

        // 设置为格式化输出（默认）
        store.set_format(config::JsonFormat::Pretty);
        store.set("formatted", "data");
        store.save();
        std::cout << "使用 Pretty 格式保存（易读）" << std::endl;

        // 设置为压缩输出
        store.set_format(config::JsonFormat::Compact);
        store.set("compact", "data");
        store.save();
        std::cout << "使用 Compact 格式保存（节省空间）" << std::endl;
    }

    system("pause");
    return 0;
}
