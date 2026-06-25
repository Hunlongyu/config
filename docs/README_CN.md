# Config Library

一个现代化的 C++20 配置管理库，基于 [nlohmann/json](https://github.com/nlohmann/json) 构建。它提供了线程安全、易用的 API，支持多种存储策略、数据混淆和变更监听。

[English Documentation](../README.md) | [API 参考文档](API_Reference_CN.md)

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

更多详细使用说明，请参考以下文档：

*   **[API 参考文档](docs/API_Reference_CN.md)**: 详细的 API 规范、类方法和枚举说明。
*   **[使用示例](docs/Examples_CN.md)**: 演示常见用例的代码片段。
*   **[构建与安装指南](docs/Build_and_Install_CN.md)**: 如何构建示例、运行测试以及集成库的说明。

## 📄 License

MIT License
