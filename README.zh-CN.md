# Config

[English](README.md) · [API 参考](docs/API.zh-CN.md) · [构建与安装](docs/BUILD.zh-CN.md) · [更新日志](CHANGELOG.md)

面向 C++20 应用的纯头文件 JSON 配置库。创建并持有 `config::Store`，指定配置文件，即可通过 nlohmann/json 读写指定类型的值。

- **类型化读写**：支持标量数值受检读取、自定义结构体、可选查询和缺失键回退。
- **保存策略**：支持自动或手动保存、数据校验、批量更新和原子文件替换。
- **分层配置**：合并默认值、配置文件和环境变量覆盖。
- **变更追踪**：提供作用域订阅和可选的文件监视器。

> 当前版本：[**0.2.2**](https://github.com/Hunlongyu/config/releases/tag/v0.2.2)。本次包含公开 API 变更，已有应用请按[迁移指南](docs/MIGRATION.zh-CN.md)调整。

## 快速开始

### 1. 接入库

需要 **C++20 编译器**和 **nlohmann/json**；使用 CMake 集成时需要 **CMake 3.28+**。将本仓库源码放在 `third_party/config`，然后链接 `config::config`：

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX)

add_subdirectory(third_party/config)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE config::config)
```

该 target 自动传递 C++20 要求、头文件路径和平台依赖。CMake 优先复用已有的 `nlohmann_json::nlohmann_json` target，其次查找已安装的包，最后下载 JSON 3.12.0。安装包、离线依赖和手动复制头文件的用法见[构建与集成](docs/BUILD.zh-CN.md)。

### 2. 读写配置

将以下代码保存为 `main.cpp`：

<!-- snippet:examples/basic.cpp:quick-start -->
```cpp
#include <config/config.hpp>
#include <iostream>

int main()
{
    config::Store store("settings.json", {.save = config::SaveMode::manual});
    store.set("/server/port", 8080);
    store.update([](nlohmann::json &values) {
        values["server"]["host"] = "localhost";
        values["theme"]          = "dark";
    });
    store.save();
    std::cout << store.get<int>("/server/port") << '\n';
    const auto fallback  = store.get<int>("/absent", 80);
    const auto generated = store.get<int>("/absent", [] { return 80; });
    const auto optional  = store.find<int>("/absent");
    return fallback == 80 && generated == 80 && !optional ? 0 : 1;
}
```
<!-- /snippet -->

示例会在当前工作目录写入 `settings.json`，并输出 `8080`。文件不存在时，以空配置开始，保存时才创建文件。

默认采用**自动保存**：每次成功修改都会先持久化，再对读取方可见。示例选择 `SaveMode::manual`，先在内存中收集修改，再调用 `save()` 统一写入。

## 配置模型

键采用 **JSON Pointer** 语法：

| 路径 | 对应的值 |
|---|---|
| `/server/port` | `server` 中的 `port` |
| `/a~1b` | 名为 `a/b` 的属性 |
| `/a~0b` | 名为 `a~b` 的属性 |
| `/` | 名称为空的属性 |

通过 `snapshot()` 读取根对象，通过 `update(fn)` 一次性修改可写层的根对象。`get<T>(key)` 会报告缺失值和类型错误；第二个参数可传入回退值或无参生成函数，**仅在键缺失时**回退。生成函数同步执行，至多一次，不持有内部锁，其结果不会被写回配置。

需要可选结果时使用 `find<T>(key)`：仅缺失返回 nullopt，类型错误仍抛错。读取结果使用 `std::string` 等拥有数据的类型。标量数值读取拒绝越界及小数转整数的截断，布尔回退必须为 bool。`erase(key)` 返回是否删除了可写值；可写键不存在时不改变存储、版本或 dirty 状态。

读取时，配置来源按以下顺序合并，优先级从低到高：

```text
默认值 → 有序文件层 → 可写文件 → 环境变量
```

保存只写入可写层。环境变量在构造和 `reload()` 时读取。支持导入普通 JSON 对象；保存时使用带版本标记的 [`config/v1` 文件格式](docs/API.zh-CN.md)。0.1 的编码文件需要[显式导入](docs/MIGRATION.zh-CN.md)。

## 行为保证与使用约定

- **失败处理**：自动修改先校验和持久化，再提交状态和通知订阅者。重载失败时保留最后有效状态。配置错误提供结构化的 `config::ErrorCode`。
- **并发修改**：提交冲突抛出 `ErrorCode::conflict`，由调用方决定是否重试。每次调用 `update()`，其回调只执行一次。
- **文件所有权**：同一链接程序映像内，每个文件共享一个 Store。跨进程访问需要由应用协调文件所有权。
- **订阅与监视**：保留返回的 `Subscription` 才能持续接收变更。包含 `<config/watcher.hpp>` 并持有 `Watcher`，即可重载外部文件修改；它会保护未保存的手动修改。
- **编码**：Base64、Hex、ROT13、反转和组合编码均可逆，**不是加密**。

错误处理、回调和生命周期的完整约定见 [API 参考](docs/API.zh-CN.md)；模块边界和验证范围见[架构与测试](docs/ARCHITECTURE.zh-CN.md)。

## 示例

| 示例 | 内容 |
|---|---|
| [基本操作](examples/basic.cpp) | 类型化读取、回退值、批量修改和手动保存 |
| [配置来源](examples/sources.cpp) | 默认值、文件层、环境变量和编码 |
| [观察变更](examples/observe.cpp) | 校验、订阅和文件监视 |
| [结构体序列化](examples/serialization.cpp) | 自定义 C++ 类型的读写 |

所有示例均参与测试套件的构建和运行；上方的快速开始片段会与实际编译的源文件校验一致性。

## 构建与测试

在仓库根目录运行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure --parallel
```

Windows 请使用 Visual Studio 开发者命令行。Sanitizer 和覆盖率选项见[构建文档](docs/BUILD.zh-CN.md)，开发流程见[贡献指南](CONTRIBUTING.md)。

## 许可证

[MIT](LICENSE.txt)。
