# Config

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake 3.28+](https://img.shields.io/badge/CMake-3.28%2B-green.svg)](https://cmake.org/)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)]()

A modern, header-only C++20 configuration management library backed by [nlohmann/json](https://github.com/nlohmann/json). It provides a zero-boilerplate global API alongside a full-featured `ConfigStore` class — both thread-safe, both persistent.

[中文文档](docs/README_CN.md) | [API Reference](docs/API_Reference.md) | [Build & Install](docs/Build_and_Install.md)

---

## Features

| Category | Capability |
|---|---|
| **API style** | Global `get`/`set` for quick use; `ConfigStore` for multiple isolated stores |
| **Paths** | JSON Pointer-style `/` separated keys for deep nested access |
| **Persistence** | Auto-save on every `set`, or Manual save via `save()` |
| **Path resolution** | Relative (cwd), Absolute, or AppData (platform-specific user config dir) |
| **Obfuscation** | Per-key encoding: Base64, Hex, ROT13, Reverse, Combined |
| **File watcher** | Background thread polls the config file and calls `reload()` on change |
| **Schema validation** | Register a validator callback; it can throw to reject invalid config |
| **Env var binding** | Prefix-based env override (`StoreOptions::env_prefix`) and explicit `bind_env()` |
| **Layered config** | `load_layered()` merges an ordered list of files (defaults → user → override) |
| **Wildcard listeners** | `on_any_change()` fires on every mutation; `connect()`/`on_change()` target one key |
| **Subtree snapshots** | `sub(prefix)` returns a point-in-time copy of any sub-tree |
| **Default value layer** | `set_default()` registers in-memory fallbacks that survive `reload()` and `clear()` |
| **Struct serialization** | Standard nlohmann/json `NLOHMANN_DEFINE_TYPE_*` macros work out of the box |
| **Thread safety** | `std::shared_mutex` guards all reads and writes |
| **RAII listeners** | `Connection` handle auto-disconnects on destruction |

---

## Integration

### CMake FetchContent (recommended)

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

nlohmann/json is fetched automatically — no separate install step required.

### Manual copy

1. Copy the `include/config` directory into your project's include path.
2. Make sure nlohmann/json is available on the include path.
3. Add `#include <config/config.hpp>` to your source files.

---

## Quick Start

### Global API

The global API is ready to use with no setup. It writes to `config.json` in the current working directory.

```cpp
#include <config/config.hpp>
#include <iostream>

int main()
{
    // Write values — auto-saved immediately
    config::set("server/host", std::string("127.0.0.1"));
    config::set("server/port", 8080);
    config::set("app/debug",   true);

    // Read values with a fallback default
    auto host  = config::get<std::string>("server/host", "localhost");
    auto port  = config::get<int>("server/port", 80);
    auto debug = config::get<bool>("app/debug", false);

    std::cout << host << ":" << port << "  debug=" << debug << "\n";
}
```

### ConfigStore API

Use `ConfigStore` when you need multiple stores, fine-grained control, or any of the advanced features.

```cpp
#include <config/config.hpp>
#include <iostream>

int main()
{
    // Store in the platform user-config directory, manual save
    config::ConfigStore store("myapp/settings.json", {
        .path_type  = config::Path::AppData,
        .save       = config::SaveStrategy::Manual,
        .env_prefix = "MYAPP_",   // MYAPP_SERVER_PORT overrides server/port
    });

    // Defaults layer — survives reload() and clear()
    store.set_default("server/port", 8080);
    store.set_default("app/theme",   std::string("dark"));

    // Schema validation — throw to reject bad config
    store.set_validator([](const nlohmann::json &cfg) {
        if (!cfg.contains("server"))
            throw std::runtime_error("Missing required section: server");
    });

    // Typed change listener with RAII auto-disconnect
    auto conn = store.on_change<int>("server/port", [](int port) {
        std::cout << "Port changed to " << port << "\n";
    });

    // File watcher — calls reload() automatically when the file changes on disk
    store.start_watch(std::chrono::seconds{2});

    store.set("server/port", 9090);
    store.save();                        // persist (Manual strategy)

    // Subtree snapshot
    auto server_cfg = store.sub("server");
    std::cout << server_cfg.dump(4) << "\n";
}
```

---

## Advanced Usage

### Layered configuration

Load files in priority order (later files win):

```cpp
store.load_layered({
    "config/defaults.json",
    "config/production.json",
    "config/local.json",     // highest priority; may not exist — silently skipped
});
```

### Obfuscation

Encode sensitive values at rest with a per-key strategy:

```cpp
store.set("auth/token", std::string("s3cr3t"), config::Encoding::Base64);
// Stored on disk as Base64; decoded transparently by get()
auto token = store.get<std::string>("auth/token");
```

### Struct serialization

Any type that nlohmann/json can serialize works directly:

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

### Wildcard listener

```cpp
auto conn = store.on_any_change([](const nlohmann::json &val) {
    std::cout << "Something changed: " << val.dump() << "\n";
});
```

### Explicit env binding

```cpp
store.bind_env("database/url", "DATABASE_URL");
store.reload();   // picks up the env var now
```

---

## API Summary

| Method | Description |
|---|---|
| `get<T>(key, default)` | Read a value with a fallback |
| `set(key, value)` | Write a value (obfuscation optional) |
| `remove(key)` | Delete a key |
| `contains(key)` | Check key existence |
| `get_or_set(key, default)` | Atomic read-or-initialize |
| `save()` / `reload()` / `clear()` | Persistence and lifecycle |
| `merge(json)` / `merge_file(path)` | Deep-merge an overlay |
| `load_layered(paths)` | Priority-ordered file stacking |
| `sub(prefix)` | Point-in-time subtree copy |
| `keys(prefix)` / `all_keys(prefix)` | Enumerate keys |
| `get_all<T>(prefix)` | All child values as a typed map |
| `connect(key, cb)` / `on_change<T>(key, cb)` | Scoped change listeners |
| `on_any_change(cb)` | Wildcard listener |
| `start_watch()` / `stop_watch()` | Background file watcher |
| `set_validator(fn)` | Schema validation hook |
| `set_default(key, value)` | In-memory default value layer |
| `bind_env(key, var)` | Explicit env variable binding |
| `dump(format)` | Serialize store to string |

Full parameter details and overloads are in [docs/API_Reference.md](docs/API_Reference.md).

---

## Requirements

- **C++20** compiler (GCC 12+, Clang 15+, MSVC 19.34+)
- **CMake 3.28+** (when using the CMake integration)
- **nlohmann/json** — fetched automatically via FetchContent when using CMake; otherwise add it manually

---

## License

MIT License
