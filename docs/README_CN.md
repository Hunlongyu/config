# Config

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake 3.28+](https://img.shields.io/badge/CMake-3.28%2B-green.svg)](https://cmake.org/)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)]()

一个现代化的纯头文件 C++20 配置管理库，基于 [nlohmann/json](https://github.com/nlohmann/json) 构建。提供零样板代码的全局 API 以及功能完整的 `ConfigStore` 类——两者均线程安全，均支持持久化存储。

[English Documentation](../README.md) | [API 参考文档](API_Reference_CN.md) | [构建与安装](Build_and_Install_CN.md)

---

## 特性

| 类别 | 能力 |
|---|---|
| **API 风格** | 全局 `get`/`set` 快速使用；`ConfigStore` 支持多个独立存储实例 |
| **路径** | 使用 `/` 分隔的 JSON Pointer 风格键名，支持深层嵌套访问 |
| **持久化** | 每次 `set` 自动保存，或通过 `save()` 手动保存 |
| **路径解析** | 相对路径（当前目录）、绝对路径，或 AppData（平台特定的用户配置目录） |
| **数据混淆** | 按键编码策略：Base64、Hex、ROT13、Reverse 及组合方式 |
| **文件监听** | 后台线程轮询配置文件，文件变更时自动调用 `reload()` |
| **Schema 校验** | 注册校验回调函数；可通过抛出异常拒绝无效配置 |
| **环境变量绑定** | 基于前缀的环境变量覆盖（`StoreOptions::env_prefix`）及显式 `bind_env()` |
| **分层配置** | `load_layered()` 按顺序合并多个文件（默认值 → 用户配置 → 覆盖配置） |
| **通配符监听** | `on_any_change()` 响应所有变更；`connect()`/`on_change()` 针对单个键 |
| **子树快照** | `sub(prefix)` 返回任意子树的时间点副本 |
| **默认值层** | `set_default()` 注册内存中的回退值，`reload()` 和 `clear()` 后仍然有效 |
| **结构体序列化** | 标准 nlohmann/json `NLOHMANN_DEFINE_TYPE_*` 宏开箱即用 |
| **线程安全** | `std::shared_mutex` 保护所有读写操作 |
| **RAII 监听器** | `Connection` 句柄在析构时自动断开连接 |

---

## 集成

### CMake FetchContent（推荐）

```cmake
include(FetchContent)

FetchContent_Declare(
  config
  GIT_REPOSITORY https://github.com/Hunlongyu/config.git
  GIT_TAG        main  # pin to a release tag in production
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(config)

target_link_libraries(your_target PRIVATE config::config)
```

nlohmann/json 会自动拉取，无需单独安装。

### 手动复制

1. 将 `include/config` 目录复制到项目的头文件搜索路径中。
2. 确保 nlohmann/json 也在头文件搜索路径中可用。
3. 在源文件中添加 `#include <config/config.hpp>`。

---

## 快速开始

### 全局 API

全局 API 无需任何初始化即可直接使用，配置文件写入当前工作目录下的 `config.json`。

```cpp
#include <config/config.hpp>
#include <iostream>

int main()
{
    // 写入值——立即自动保存
    config::set("server/host", std::string("127.0.0.1"));
    config::set("server/port", 8080);
    config::set("app/debug",   true);

    // 读取值，支持回退默认值
    auto host  = config::get<std::string>("server/host", "localhost");
    auto port  = config::get<int>("server/port", 80);
    auto debug = config::get<bool>("app/debug", false);

    std::cout << host << ":" << port << "  debug=" << debug << "\n";
}
```

### ConfigStore API

当需要多个存储实例、精细控制或任何高级功能时，请使用 `ConfigStore`。

```cpp
#include <config/config.hpp>
#include <iostream>

int main()
{
    // 存储到平台用户配置目录，手动保存模式
    config::ConfigStore store("myapp/settings.json", {
        .path_type  = config::Path::AppData,
        .save       = config::SaveStrategy::Manual,
        .env_prefix = "MYAPP_",   // MYAPP_SERVER_PORT 会覆盖 server/port
    });

    // 默认值层——reload() 和 clear() 后仍然有效
    store.set_default("server/port", 8080);
    store.set_default("app/theme",   std::string("dark"));

    // Schema 校验——抛出异常可拒绝无效配置
    store.set_validator([](const nlohmann::json &cfg) {
        if (!cfg.contains("server"))
            throw std::runtime_error("Missing required section: server");
    });

    // 带类型的变更监听器，支持 RAII 自动断开
    auto conn = store.on_change<int>("server/port", [](int port) {
        std::cout << "Port changed to " << port << "\n";
    });

    // 文件监听——磁盘文件变更时自动调用 reload()
    store.start_watch(std::chrono::seconds{2});

    store.set("server/port", 9090);
    store.save();                        // 持久化（手动保存模式）

    // 子树快照
    auto server_cfg = store.sub("server");
    std::cout << server_cfg.dump(4) << "\n";
}
```

---

## 高级用法

### 分层配置

按优先级顺序加载文件（后面的文件优先级更高）：

```cpp
store.load_layered({
    "config/defaults.json",
    "config/production.json",
    "config/local.json",     // highest priority; may not exist — silently skipped
});
```

### 数据混淆

使用按键混淆策略对敏感值进行静态编码：

```cpp
store.set("auth/token", std::string("s3cr3t"), config::Encoding::Base64);
// 以 Base64 形式存储到磁盘；get() 读取时透明解码
auto token = store.get<std::string>("auth/token");
```

### 结构体序列化

所有 nlohmann/json 可序列化的类型均可直接使用：

```cpp
struct ServerConfig {
    std::string host;
    int port;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ServerConfig, host, port)
};

ServerConfig cfg{ "localhost", 8080 };
store.set("server", cfg);
auto loaded = store.get<ServerConfig>("server");
```

### 通配符监听器

```cpp
auto conn = store.on_any_change([](const nlohmann::json &val) {
    std::cout << "Something changed: " << val.dump() << "\n";
});
```

### 显式环境变量绑定

```cpp
store.bind_env("database/url", "DATABASE_URL");
store.reload();   // picks up the env var now
```

---

## API 概览

| 方法 | 说明 |
|---|---|
| `get<T>(key, default)` | 读取值，支持回退默认值 |
| `set(key, value)` | 写入值（可选混淆） |
| `remove(key)` | 删除键 |
| `contains(key)` | 检查键是否存在 |
| `get_or_set(key, default)` | 原子性读取或初始化 |
| `save()` / `reload()` / `clear()` | 持久化与生命周期管理 |
| `merge(json)` / `merge_file(path)` | 深度合并覆盖层 |
| `load_layered(paths)` | 按优先级顺序加载多个文件 |
| `sub(prefix)` | 获取子树的时间点副本 |
| `keys(prefix)` / `all_keys(prefix)` | 枚举键名 |
| `get_all<T>(prefix)` | 以类型化 map 形式获取所有子值 |
| `connect(key, cb)` / `on_change<T>(key, cb)` | 作用域变更监听器 |
| `on_any_change(cb)` | 通配符监听器 |
| `start_watch()` / `stop_watch()` | 后台文件监听 |
| `set_validator(fn)` | Schema 校验钩子 |
| `set_default(key, value)` | 内存默认值层 |
| `bind_env(key, var)` | 显式环境变量绑定 |
| `dump(format)` | 将存储序列化为字符串 |

完整的参数说明和重载列表请参阅 [docs/API_Reference_CN.md](docs/API_Reference_CN.md)。

---

## 环境要求

- **C++20** 编译器（GCC 12+、Clang 15+、MSVC 19.34+）
- **CMake 3.28+**（使用 CMake 集成时）
- **nlohmann/json**——使用 CMake 时通过 FetchContent 自动获取；否则需手动添加

---

## 许可证

MIT License
