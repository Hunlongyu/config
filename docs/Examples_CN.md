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
