# 使用示例

本文档提供了涵盖库各种功能的详细示例。您可以在 [examples/](../examples/) 目录中找到这些示例的完整源代码。

## 1. 基础用法 (Basic Usage)
[源码: examples/01_basic_usage.cpp](../examples/01_basic_usage.cpp)

展示如何进行基本的 `get` 和 `set` 操作，包括类型安全和默认值处理。

```cpp
// 设置值
config::set("app/name", "MyApp");
config::set("app/version", 1);

// 获取值
std::string name = config::get<std::string>("app/name");
int version = config::get<int>("app/version", 0); // 如果不存在则默认为 0
```

## 2. 保存策略 (Save Strategies)
[源码: examples/02_save_strategies.cpp](../examples/02_save_strategies.cpp)

演示 `Auto`（自动，默认）和 `Manual`（手动）保存模式的区别。

```cpp
// 切换到手动模式以进行批量更新
config::set_save_strategy(config::SaveStrategy::Manual);

for (int i = 0; i < 100; ++i) {
    config::set("data/" + std::to_string(i), i);
}

config::save(); // 仅在此时写入磁盘一次
```

## 3. 读取策略 (Get Strategies)
[源码: examples/03_get_strategies.cpp](../examples/03_get_strategies.cpp)

展示如何处理缺失的键：返回默认值 vs. 抛出异常。

```cpp
config::set_get_strategy(config::GetStrategy::ThrowException);

try {
    auto val = config::get<int>("missing_key");
} catch (const std::exception& e) {
    std::cerr << "捕获到预期异常: " << e.what() << std::endl;
}
```

## 4. 数据混淆 (Data Obfuscation)
[源码: examples/04_obfuscation.cpp](../examples/04_obfuscation.cpp)

使用内置混淆策略保护密码等敏感数据。

```cpp
// 加密存储
config::set("db/password", "SecretPass", config::Obfuscate::Base64);

// 解密读取 (透明处理)
std::string pass = config::get<std::string>("db/password");
```

## 5. 路径解析 (Path Resolution)
[源码: examples/05_path_strategies.cpp](../examples/05_path_strategies.cpp)

将配置文件保存到系统标准位置 (AppData/Home)。

```cpp
// 在 Windows 上自动解析为 %APPDATA%/MyApp/user.json
auto& store = config::get_store("user.json", config::Path::AppData);
```

## 6. 监听器 (Listeners)
[源码: examples/06_listeners.cpp](../examples/06_listeners.cpp)

实时响应配置更改。

```cpp
config::connect("theme", [](const nlohmann::json& val) {
    std::cout << "主题变更为: " << val << std::endl;
});

config::set("theme", "Dark"); // 触发回调
```

## 7. 全局函数 (Global Functions)
[源码: examples/07_global_functions.cpp](../examples/07_global_functions.cpp)

演示便捷的全局 API 封装。

## 8. 并发访问 (Concurrent Access)
[源码: examples/08_concurrent_access.cpp](../examples/08_concurrent_access.cpp)

演示多线程同时对 `ConfigStore` 进行线程安全的读写操作，使用 `SaveStrategy::Manual` 避免磁盘 I/O，专注于并发正确性验证。

```cpp
#include <config/config.hpp>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "=== 并发访问演示 ===" << std::endl;

    // 使用 Manual 策略避免磁盘 I/O，专注演示线程安全性
    auto store = config::ConfigStore("08_concurrent.json", config::Path::Relative, config::SaveStrategy::Manual);

    // 初始化共享数据
    store.set("counter", 0);
    store.set("message", std::string("hello"));
    store.set("read_count", 0);

    std::cout << "\n=== 启动读写线程 ===" << std::endl;

    // 创建 5 个读线程
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; ++i)
    {
        readers.emplace_back([&store, i]() {
            for (int j = 0; j < 10; ++j)
            {
                [[maybe_unused]] auto counter = store.get<int>("counter");
                [[maybe_unused]] auto message = store.get<std::string>("message");
                // 模拟一些处理时间
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout << "读线程 " << i << " 完成\n";
        });
    }

    // 创建 1 个写线程
    std::thread writer([&store]() {
        for (int i = 1; i <= 10; ++i)
        {
            store.set("counter", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::cout << "写线程完成\n";
    });

    // 等待所有线程完成
    writer.join();
    for (auto &t : readers)
    {
        t.join();
    }

    std::cout << "\n=== 并发访问完成 ===" << std::endl;
    std::cout << "最终计数器值: " << store.get<int>("counter") << "\n";
    std::cout << "消息: " << store.get<std::string>("message") << "\n";
    std::cout << "线程安全演示成功 — 无数据竞争！\n";

    return 0;
}
```

## 9. 结构体序列化 (Struct Serialization)
[源码: examples/09_struct_serialization.cpp](../examples/09_struct_serialization.cpp)

展示如何使用 `CONFIG_STRUCT` 和 `CONFIG_STRUCT_WITH_DEFAULT` 宏将 C++ 结构体绑定到 JSON，从而支持整体结构体的读取（`get_root`）和写入（`set_root`）。`CONFIG_STRUCT` 要求 JSON 中必须存在所有字段，而 `CONFIG_STRUCT_WITH_DEFAULT` 在字段缺失时会回退到结构体中定义的默认值。

```cpp
#include <config/config.hpp>
#include <iostream>
#include <vector>

struct Item
{
    std::string name;
    std::string value;
};

struct Config
{
    std::string app_name;
    int version;
    std::vector<Item> items;
};

// 严格绑定：JSON 中必须存在所有字段
CONFIG_STRUCT(Item, name, value)
CONFIG_STRUCT(Config, app_name, version, items)

// 宽松绑定：缺失字段回退到结构体默认值
struct ServerConfig
{
    std::string host = "localhost";
    int port         = 8080;
    bool tls         = false;
};
CONFIG_STRUCT_WITH_DEFAULT(ServerConfig, host, port, tls)

int main()
{
    auto &store = config::get_store("example_struct.json", config::Path::Relative);

    // 1. 逐字段写入（SaveStrategy::Auto 在每次 set 时自动保存）
    store.set("app_name", "StructApp");
    store.set("version", 1);
    std::vector<Item> items = {{"item1", "value1"}, {"item2", "value2"}};
    store.set("items", items);

    // 2. 通过根键 "" 将数据读取为结构体
    try
    {
        auto cfg = store.get_root<Config>();
        std::cout << "App: " << cfg.app_name << " v" << cfg.version << "\n";
        for (const auto &item : cfg.items)
            std::cout << "  " << item.name << ": " << item.value << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // 3. 往返测试：将整个结构体写回根节点
    Config updated{"StructApp", 2, {{"item3", "value3"}}};
    store.set_root(updated);

    // 4. CONFIG_STRUCT_WITH_DEFAULT：仅设置 "host"，port 和 tls 使用默认值
    auto &srv = config::get_store("example_server.json", config::Path::Relative);
    srv.set("host", std::string("example.com"));
    auto sc = srv.get_root<ServerConfig>();
    std::cout << "Server: " << sc.host << ":" << sc.port << (sc.tls ? " (TLS)" : "") << "\n";

    return 0;
}
```
