#include <config/config.hpp>
#include <iostream>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    auto &store = config::get_store("get_strategy.json");

    // 设置一些测试数据
    store.set("existing_string", "hello");
    store.set("existing_int", 42);
    store.set("existing_bool", true);

    std::cout << "=== DefaultValue 策略（默认） ===" << std::endl;
    {
        store.set_get_strategy(config::GetStrategy::DefaultValue);

        // 键存在时返回实际值
        auto str = store.get<std::string>("existing_string");
        std::cout << "existing_string: " << str << std::endl;

        // 键不存在时返回类型默认值
        auto missing_str  = store.get<std::string>("missing_string");
        auto missing_int  = store.get<int>("missing_int");
        auto missing_bool = store.get<bool>("missing_bool");

        std::cout << "missing_string: '" << missing_str << "' (空字符串)" << std::endl;
        std::cout << "missing_int: " << missing_int << " (0)" << std::endl;
        std::cout << "missing_bool: " << missing_bool << " (false)" << std::endl;
    }

    std::cout << "\n=== ThrowException 策略 ===" << std::endl;
    {
        store.set_get_strategy(config::GetStrategy::ThrowException);

        // 键存在时正常返回
        try
        {
            auto str = store.get<std::string>("existing_string");
            std::cout << "existing_string: " << str << std::endl;
        }
        catch (const std::runtime_error &e)
        {
            std::cerr << "错误: " << e.what() << std::endl;
        }

        // 键不存在时抛出异常
        try
        {
            auto missing = store.get<std::string>("missing_key");
            std::cout << "这行不会执行" << std::endl;
        }
        catch (const std::runtime_error &e)
        {
            std::cout << "捕获异常: " << e.what() << std::endl;
        }
    }

    std::cout << "\n=== 带默认值的 get（推荐） ===" << std::endl;
    {
        // 带默认值的 get 不受策略影响
        store.set_get_strategy(config::GetStrategy::ThrowException);

        // 即使是异常策略，也不会抛异常
        auto theme = store.get<std::string>("ui/theme", "light");
        auto port  = store.get<int>("server/port", 8080);
        auto debug = store.get<bool>("debug/enabled", false);

        std::cout << "Theme: " << theme << " (使用默认值)" << std::endl;
        std::cout << "Port: " << port << " (使用默认值)" << std::endl;
        std::cout << "Debug: " << debug << " (使用默认值)" << std::endl;
    }

    std::cout << "\n=== 配置验证场景 ===" << std::endl;
    {
        store.set_get_strategy(config::GetStrategy::ThrowException);

        std::vector<std::string> required_keys = {
            "existing_string", "existing_int",
            "missing_required" // 故意缺失
        };

        for (const auto &key : required_keys)
        {
            try
            {
                auto value = store.get<std::string>(key);
                std::cout << "✓ " << key << " = " << value << std::endl;
            }
            catch (const std::runtime_error &e)
            {
                std::cout << "✗ " << key << " - " << e.what() << std::endl;
            }
        }
    }

    system("pause");
    return 0;
}
