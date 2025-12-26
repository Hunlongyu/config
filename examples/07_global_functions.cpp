#include <config/config.hpp>
#include <iostream>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "=== 全局配置函数 ===" << std::endl;

    // 全局配置默认使用 "config.json" 和相对路径

    // 设置数据
    config::set("app/name", "GlobalApp");
    config::set("app/version", "2.0.0");
    config::set("server/host", "localhost");
    config::set("server/port", 9000);

    // 读取数据（带默认值）
    auto app_name = config::get<std::string>("app/name", "Unknown");
    auto port     = config::get<int>("server/port", 8080);

    std::cout << "App: " << app_name << std::endl;
    std::cout << "Port: " << port << std::endl;

    // 检查键
    if (config::contains("server/host"))
    {
        std::cout << "Host: " << config::get<std::string>("server/host") << std::endl;
    }

    // 删除键
    config::remove("server/host");

    std::cout << "\n=== 全局策略设置 ===" << std::endl;

    // 设置保存策略
    config::set_save_strategy(config::SaveStrategy::Manual);
    config::set("temp1", "data1");
    config::set("temp2", "data2");
    config::save(); // 手动保存

    // 设置 Get 策略
    config::set_get_strategy(config::GetStrategy::ThrowException);
    try
    {
        auto missing = config::get<std::string>("missing_key");
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "捕获异常: " << e.what() << std::endl;
    }

    // 设置格式
    config::set_format(config::JsonFormat::Compact);
    config::save(config::JsonFormat::Compact);

    std::cout << "\n=== 全局配置路径 ===" << std::endl;
    std::cout << "配置文件: " << config::get_store_path() << std::endl;

    std::cout << "\n=== 重新加载配置 ===" << std::endl;
    config::set("before_reload", "value");
    config::save();

    // 模拟外部修改（实际应用中可能是其他程序修改）
    config::set("after_reload", "new_value");

    config::reload(); // 重新从文件加载
    std::cout << "配置已重新加载" << std::endl;

    std::cout << "\n=== 清空配置 ===" << std::endl;
    // 注意：当前策略为 Manual，clear() 只清空内存，不影响文件
    // 除非手动调用 save()
    config::clear();
    std::cout << "配置内存已清空（策略为 Manual，文件未变更）" << std::endl;

    // 如果要清空文件：
    // config::save();

    // 切换回自动保存策略
    config::set_save_strategy(config::SaveStrategy::Auto);
    config::set("new_start", "value"); // 自动写入新数据，覆盖原文件内容（因为内存已空）

    system("pause");
    return 0;
}
