# Config Library

一个现代化的 C++20 配置管理库，基于 [nlohmann/json](https://github.com/nlohmann/json) 构建。它提供了线程安全、易用的 API，支持多种存储策略、数据混淆和变更监听。

## ✨ 特性

- **Header-only**: 只需包含头文件即可使用。
- **简单易用**: 提供类似 map 的全局 `get`/`set` 接口，开箱即用。
- **JSON Pointer**: 支持使用 `/` 分隔的路径访问深层嵌套数据（如 `user/profile/name`）。
- **多种存储策略**:
  - **自动保存 (`Auto`)**: 每次修改立即写入文件。
  - **手动保存 (`Manual`)**: 显式调用 `save()` 时才写入。
- **灵活的路径管理**:
  - **相对路径**: 相对于应用程序运行目录。
  - **绝对路径**: 指定完整文件路径。
  - **系统路径 (`AppData`)**: 自动适配 Windows (%LOCALAPPDATA%), macOS (~/Library), Linux (~/.config)。
- **数据混淆**: 内置 Base64, Hex, ROT13, Reverse 及其组合策略，保护敏感配置。
- **变更监听**: 支持注册回调函数，实时监听配置项的变化。
- **线程安全**: 全局接口和 `ConfigStore` 实例均保证线程安全。

## 📦 集成

### 要求
- C++20 编译器
- CMake 3.28+ (如果使用 CMake 集成)

### 方法 1: CMake FetchContent (推荐)

在你的 `CMakeLists.txt` 中添加：

```cmake
include(FetchContent)

FetchContent_Declare(
  config
  GIT_REPOSITORY https://github.com/Hunlongyu/config.git
  GIT_TAG main  # 建议指定具体的 tag
)
FetchContent_MakeAvailable(config)

target_link_libraries(your_target PRIVATE config::config)
```

### 方法 2: 手动集成

1. 确保你的项目中包含 `nlohmann/json`。
2. 将 `include/config` 目录复制到你的项目包含路径中。
3. 在代码中 `#include <config/config.hpp>`。

## 🚀 快速开始

```cpp
#include <config/config.hpp>
#include <iostream>

int main() {
    // 1. 设置配置 (自动保存到运行目录下的 config.json)
    config::set("server/host", "127.0.0.1");
    config::set("server/port", 8080);
    config::set("app/debug", true);

    // 2. 读取配置 (支持默认值)
    std::string host = config::get<std::string>("server/host", "localhost");
    int port = config::get<int>("server/port", 80);
    
    std::cout << "Server running at " << host << ":" << port << std::endl;

    return 0;
}
```

## 📖 详细指南

### 1. 全局配置 vs 独立实例

**全局模式**: 最简单的用法，默认操作 `config.json`。

```cpp
config::set("key", "value");
auto val = config::get<std::string>("key");
```

**独立实例**: 当你需要管理多个配置文件时。

```cpp
// 获取或创建一个名为 "user_settings.json" 的配置存储
// 使用 AppData 策略时，会自动创建: %LOCALAPPDATA%/<ExeName>/user_settings.json
auto& store = config::get_store("user_settings.json", config::Path::AppData);

store.set("theme", "dark");
store.save();
```

### 2. 路径策略 (`config::Path`)

- **Path::Relative** (默认): 文件保存在程序运行目录。
- **Path::Absolute**: 使用完整的文件系统路径。
- **Path::AppData**: 自动根据操作系统选择合适的配置目录，并隔离不同程序。
  - Windows: `%LOCALAPPDATA%/<ExeName>/<Path>`
  - Linux: `~/.config/<ExeName>/<Path>`
  - macOS: `~/Library/Application Support/<ExeName>/<Path>`

### 3. 保存策略 (`config::SaveStrategy`)

- **SaveStrategy::Auto** (默认): 每次 `set` 操作后自动写入磁盘。
- **SaveStrategy::Manual**: 仅在调用 `save()` 时写入，适合频繁修改的场景。

```cpp
// 全局设置
config::set_save_strategy(config::SaveStrategy::Manual);

config::set("a", 1);
config::set("b", 2);
config::save(); // 此时才写入文件
```

### 4. 数据混淆 (`config::Obfuscate`)

支持对字符串类型的值进行混淆存储，防止直接文本阅读。

支持的算法: `None`, `Base64`, `Hex`, `ROT13`, `Reverse`, `Combined` (Base64 + Reverse)。

```cpp
// 写入时指定混淆策略
config::set("db/password", "secret123", config::Obfuscate::Base64);

// 读取时自动解密，无需额外参数
std::string pwd = config::get<std::string>("db/password");
```

### 5. 变更监听

你可以监听特定 key 或路径的变化。

```cpp
auto& store = config::get_store("config.json");

size_t listener_id = store.connect("server/status", [](const nlohmann::json& val) {
    std::cout << "Server status changed to: " << val << std::endl;
});

store.set("server/status", "running"); // 触发回调
```

### 6. 读取策略 (`config::GetStrategy`)

- **DefaultValue** (默认): Key 不存在时返回提供的默认值或类型默认值。
- **ThrowException**: Key 不存在时抛出 `std::runtime_error`。

```cpp
config::set_get_strategy(config::GetStrategy::ThrowException);

try {
    config::get<int>("non_existent_key");
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## 🛠️ 构建示例与测试

本项目包含丰富的示例和单元测试。

```bash
mkdir build && cd build
cmake .. -DBUILD_CONFIG_EXAMPLES=ON -DBUILD_TESTING=ON
cmake --build .
```

- **示例**: 生成在 `build/examples/` 目录下。
- **测试**: 运行 `ctest -C Release`。

## 📄 License

MIT License
