# Usage Examples

This document provides detailed examples covering various features of the library. You can find the full source code for these examples in the [examples/](../examples/) directory.

## 1. Basic Usage
[Source: examples/01_basic_usage.cpp](../examples/01_basic_usage.cpp)

Shows how to perform basic `get` and `set` operations with type safety and default values.

```cpp
// Set values
config::set("app/name", "MyApp");
config::set("app/version", 1);

// Get values
std::string name = config::get<std::string>("app/name");
int version = config::get<int>("app/version", 0); // Default to 0 if missing
```

## 2. Save Strategies
[Source: examples/02_save_strategies.cpp](../examples/02_save_strategies.cpp)

Demonstrates the difference between `Auto` (default) and `Manual` save modes.

```cpp
// Switch to Manual mode for batch updates
config::set_save_strategy(config::SaveStrategy::Manual);

for (int i = 0; i < 100; ++i) {
    config::set("data/" + std::to_string(i), i);
}

config::save(); // Write to disk only once
```

## 3. Get Strategies
[Source: examples/03_get_strategies.cpp](../examples/03_get_strategies.cpp)

Shows how to handle missing keys: return default value vs. throw exception.

```cpp
config::set_get_strategy(config::GetStrategy::ThrowException);

try {
    auto val = config::get<int>("missing_key");
} catch (const std::exception& e) {
    std::cerr << "Caught expected error: " << e.what() << std::endl;
}
```

## 4. Data Obfuscation
[Source: examples/04_obfuscation.cpp](../examples/04_obfuscation.cpp)

Protect sensitive data like passwords using built-in obfuscation.

```cpp
// Store encrypted
config::set("db/password", "SecretPass", config::Obfuscate::Base64);

// Read decrypted (transparently)
std::string pass = config::get<std::string>("db/password");
```

## 5. Path Resolution
[Source: examples/05_path_strategies.cpp](../examples/05_path_strategies.cpp)

Save config files to system-standard locations (AppData/Home).

```cpp
// Automatically resolves to %APPDATA%/MyApp/user.json on Windows
auto& store = config::get_store("user.json", config::Path::AppData);
```

## 6. Listeners
[Source: examples/06_listeners.cpp](../examples/06_listeners.cpp)

React to configuration changes in real-time.

```cpp
config::connect("theme", [](const nlohmann::json& val) {
    std::cout << "Theme changed to: " << val << std::endl;
});

config::set("theme", "Dark"); // Triggers callback
```

## 7. Global Functions
[Source: examples/07_global_functions.cpp](../examples/07_global_functions.cpp)

Demonstrates the convenient global API wrappers.

## 8. Concurrent Access
[Source: examples/08_concurrent_access.cpp](../examples/08_concurrent_access.cpp)

Demonstrates thread-safe reads and writes to a `ConfigStore` from multiple threads simultaneously, using `SaveStrategy::Manual` to avoid disk I/O and focus on concurrency correctness.

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

## 9. Struct Serialization
[Source: examples/09_struct_serialization.cpp](../examples/09_struct_serialization.cpp)

Shows how to bind C++ structs to JSON using the `CONFIG_STRUCT` and `CONFIG_STRUCT_WITH_DEFAULT` macros, enabling whole-struct reads (`get_root`) and writes (`set_root`). `CONFIG_STRUCT` requires all fields to be present in the JSON, while `CONFIG_STRUCT_WITH_DEFAULT` falls back to struct-defined defaults for missing fields.

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

// Strict binding: all fields must be present in JSON
CONFIG_STRUCT(Item, name, value)
CONFIG_STRUCT(Config, app_name, version, items)

// Tolerant binding: missing fields fall back to struct defaults
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

    // 1. Write individual fields (SaveStrategy::Auto saves on each set)
    store.set("app_name", "StructApp");
    store.set("version", 1);
    std::vector<Item> items = {{"item1", "value1"}, {"item2", "value2"}};
    store.set("items", items);

    // 2. Read back as a struct via root key ""
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

    // 3. Round-trip: write a whole struct back to root
    Config updated{"StructApp", 2, {{"item3", "value3"}}};
    store.set_root(updated);

    // 4. CONFIG_STRUCT_WITH_DEFAULT: only "host" is set, port and tls use defaults
    auto &srv = config::get_store("example_server.json", config::Path::Relative);
    srv.set("host", std::string("example.com"));
    auto sc = srv.get_root<ServerConfig>();
    std::cout << "Server: " << sc.host << ":" << sc.port << (sc.tls ? " (TLS)" : "") << "\n";

    return 0;
}
```
