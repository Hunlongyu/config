# Config Store

一个现代化的 C++ 配置管理库，支持 JSON、自动/定时保存、数据混淆、路径策略与变更监听。

## 特性

- JSON 配置，兼容 JSON Pointer
- 保存策略：自动、手动、批量、定时
- 保存格式：格式化与压缩输出
- 数据混淆：Base64、XOR、字符位移、组合
- 路径策略：当前目录与 AppData 自动选择
- 变更监听与线程安全
- Header-Only，易集成（CMake INTERFACE）

## 安装与依赖

- 依赖：C++20、nlohmann/json、Windows/Linux/macOS
- CMake FetchContent（推荐）：

```cmake
include(FetchContent)
FetchContent_Declare(config GIT_REPOSITORY https://github.com/Hunlongyu/config.git GIT_TAG main)
FetchContent_MakeAvailable(config)
target_link_libraries(your_target PRIVATE config)
```

- 手动安装：复制 `include/config/config.h` 到项目并在编译选项中加入头文件路径。

## 快速开始

```cpp
#include <config/config.h>

config::set("username", "ZhangSan");
config::set("port", 8080);

auto username = config::get<std::string>("username");
auto port = config::get_or<int>("port", 3000);
```

### 多存储与 JSON Pointer

```cpp
auto db = config::get_store("database");
db->set("host", "localhost");
config::set("/server/port", 8080);
auto host = config::get<std::string>("/server/host");
```

## 保存

### 保存策略

```cpp
using namespace config;
auto s1 = get_store("app", SavePolicy::AutoSave);
auto s2 = get_store("cache", SavePolicy::ManualSave); s2->save();
auto s3 = get_store("logs", SavePolicy::TimedSave);
```

### 保存格式

```cpp
using namespace config;
config::set_save_format(SaveFormat::Compressed);
config::save();
config::save(SaveFormat::Formatted);
auto s = get_store("artifact"); s->set_save_format(SaveFormat::Compressed); s->save();
```

## 路径策略

```cpp
using namespace config;
auto s1 = get_store("app", SavePolicy::AutoSave, Path::AutoDetect);
auto s2 = get_store("user", SavePolicy::AutoSave, Path::AppData);
auto s3 = get_store("local", SavePolicy::AutoSave, Path::CurrentDir);
```

## 数据混淆

```cpp
using namespace config;
config::set_obfuscated("api_key", "sk-xxx");
config::set("password", "secret", Obfuscate::Combined);
```

## 变更监听

```cpp
auto store = config::get_store("app");
auto id = store->connect("username", [](const auto& old_val, const auto& new_val){
    std::cout << old_val << " -> " << new_val << std::endl;
});
store->disconnect(id);
```

## 示例与构建

- 打开 `examples/`，启用 `BUILD_CONFIG_EXAMPLES`（默认 ON）即可构建所有示例。
- 可执行文件输出在 `out/build/<config>/bin/`。

## 许可证与贡献

- 许可证：MIT
- 欢迎提交 Issue 和 Pull Request

## 注意事项

- 跨平台支持（Windows/Linux/macOS）
- 需要 C++20 编译器
- 混淆为基础保护，不适用于高安全性场景