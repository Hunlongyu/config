# Config Store

一个现代化的 C++ 配置管理库，支持 JSON 格式、自动保存、数据混淆和变更监听。

## 特性

- 🔧 **JSON 格式配置** - 使用 nlohmann/json 库，支持 JSON Pointer 语法
- 💾 **多种保存策略** - 自动保存、手动保存、批量保存、定时保存
- 🛡️ **数据混淆** - 支持多种混淆策略保护敏感配置
- 📂 **智能路径管理** - 自动选择最佳配置目录（AppData/当前目录）
- 🔔 **变更监听** - 支持配置变更回调通知
- 🧵 **线程安全** - 使用读写锁保证并发安全
- 📦 **Header-Only** - 仅头文件实现，易于集成

## 安装

### 使用 CMake FetchContent（推荐）

```cmake
include(FetchContent)

FetchContent_Declare(
  config
  GIT_REPOSITORY https://github.com/Hunlongyu/config.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(config)

target_link_libraries(your_target PRIVATE config)
```

### 手动安装

```bash
git clone https://github.com/Hunlongyu/config.git
# 将 config.h 复制到你的项目中
```

## 依赖

- C++20 或更高版本
- nlohmann/json
- Windows SDK（当前仅支持 Windows）

## 快速开始

### 基本用法

```cpp
#include "config.hpp"

// 使用全局配置
config::set("username", "张三");
config::set("port", 8080);
config::set("debug", true);

// 获取配置
auto username = config::get<std::string>("username");
auto port = config::get_or<int>("port", 3000);  // 默认值
```

### 使用命名配置存储

```cpp
// 创建多个独立的配置存储
auto db_config = config::get_store("database");
db_config->set("host", "localhost");
db_config->set("port", 5432);
db_config->set("password", "secret123");

auto server_config = config::get_store("server");
server_config->set("bind_address", "0.0.0.0");
server_config->set("max_connections", 1000);

auto ui_config = config::get_store("ui_settings");
ui_config->set("theme", "dark");
ui_config->set("language", "zh-CN");

// 每个存储都有独立的配置文件
// database.json, server.json, ui_settings.json
```

### JSON Pointer 支持

```cpp
// 使用 JSON Pointer 语法访问嵌套配置
config::set("/server/host", "localhost");
config::set("/server/port", 8080);
config::set("/database/connections/0/host", "db1.example.com");

auto host = config::get<std::string>("/server/host");
```

### 数据混淆

```cpp
// 设置敏感数据时使用混淆
config::set_obfuscated("api_key", "sk-1234567890abcdef");
config::set("password", "secret123", config::Obfuscate::combined);

// 获取时自动解混淆
auto api_key = config::get<std::string>("api_key");
```

### 变更监听

```cpp
auto store = config::get_store("app");

// 监听配置变更
auto listener_id = store->connect("username", [](const auto& old_val, const auto& new_val) {
    std::cout << "用户名从 " << old_val << " 更改为 " << new_val << std::endl;
});

// 取消监听
store->disconnect(listener_id);
```

## 保存策略

```cpp
using namespace config;

// 自动保存（默认）
auto config1 = get_store("app", save_policy::auto_save);

// 手动保存
auto config2 = get_store("cache", save_policy::manual_save);
config2->set("key", "value");
config2->save();  // 手动保存

// 定时保存
auto config3 = get_store("logs", save_policy::timed_save);
```

## 路径策略

```cpp
using namespace config;

// 自动检测路径（优先 AppData）
auto config1 = get_store("app", save_policy::auto_save, Path::auto_detect);

// 强制使用 AppData 目录
auto config2 = get_store("user", save_policy::auto_save, Path::appdata);

// 使用当前目录
auto config3 = get_store("local", save_policy::auto_save, Path::current_dir);
```

## 混淆策略

- `Obfuscate::none` - 无混淆
- `Obfuscate::base64` - Base64 编码
- `Obfuscate::xor_cipher` - XOR 异或混淆
- `Obfuscate::char_shift` - 字符位移混淆
- `Obfuscate::combined` - 组合混淆（推荐）

## 更多示例

查看 `examples/` 目录获取完整的使用示例。

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request！

## 注意事项

- 当前版本仅支持 Windows 平台
- 需要 C++20 编译器支持
- 混淆功能仅为基础保护，不适用于高安全性要求的场景