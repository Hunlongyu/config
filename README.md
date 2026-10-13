# Config

[中文](README.zh-CN.md) · [API reference](docs/API.md) · [Build & install](docs/BUILD.md) · [Changelog](CHANGELOG.md)

A header-only JSON configuration library for C++20 applications. Own a `config::Store`, choose a file, and read or update typed values with nlohmann/json.

- **Typed access** — read values and custom structs, with explicit fallbacks for missing keys.
- **Controlled persistence** — automatic or manual saving, validation, batch updates and atomic file replacement.
- **Layered configuration** — combine defaults, configuration files and environment overrides.
- **Change tracking** — scoped subscriptions and an optional file watcher.

> [**0.2.0**](https://github.com/Hunlongyu/config/releases/tag/v0.2.0) changes the public API. For existing applications, follow the [0.1 → 0.2 migration guide](docs/MIGRATION.md).

## Quick start

### 1. Add the library

Requires a **C++20 compiler** and **nlohmann/json**; CMake integration requires **CMake 3.28+**. Place this checkout under `third_party/config`, then link `config::config`:

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX)

add_subdirectory(third_party/config)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE config::config)
```

The target supplies C++20, include paths and platform dependencies. CMake reuses an existing `nlohmann_json::nlohmann_json` target, finds an installed package, or fetches JSON 3.11.3. See [build and integration](docs/BUILD.md) for installed packages, offline use and manual header copying.

### 2. Read and write configuration

Save this as `main.cpp`:

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
    return store.get_or<int>("/absent", 80) == 80 ? 0 : 1;
}
```
<!-- /snippet -->

This example writes `settings.json` in the current working directory and prints `8080`. A missing file starts with an empty configuration and is created on save.

The default is **automatic saving**: each successful mutation is persisted before becoming visible. The example selects `SaveMode::manual` to collect edits in memory and write them with `save()`.

## Configuration model

Keys use **JSON Pointer** syntax:

| Pointer | Value addressed |
|---|---|
| `/server/port` | `port` inside `server` |
| `/a~1b` | Property named `a/b` |
| `/a~0b` | Property named `a~b` |
| `/` | Property with an empty name |

Use `snapshot()` to read the root object and `update(fn)` to edit the writable root in one batch. `get<T>` reports missing values and type errors; `get_or<T>` uses its fallback **only when the key is absent**.

Reads combine sources in increasing priority:

```text
defaults → ordered file layers → writable file → environment
```

Only the writable layer is saved. Environment values are sampled at construction and on `reload()`. Existing plain JSON objects can be imported; saves use the versioned [`config/v1` envelope](docs/API.md#errors-and-persistence). Encoded 0.1 files need [explicit import](docs/MIGRATION.md).

## Guarantees and usage notes

- **Failure handling:** automatic mutations validate and persist before committing state or notifying subscribers. Failed reloads retain the last valid state. Configuration errors include a structured `config::ErrorCode`.
- **Concurrent updates:** a conflicting commit throws `ErrorCode::conflict`; callers decide whether to retry. An `update()` callback runs once per call.
- **File ownership:** share one Store per file within a linked program image. Coordination between processes requires application-level ownership.
- **Subscriptions and watching:** keep the returned `Subscription` to receive changes. Include `<config/watcher.hpp>` and hold a `Watcher` to reload external file edits; it preserves unsaved manual edits.
- **Encoding:** Base64, Hex, ROT13, reverse and combined encodings are reversible representations, **not encryption**.

See the [API reference](docs/API.md) for error, callback and lifetime contracts, and [architecture and testing](docs/ARCHITECTURE.md) for implementation boundaries and verification scope.

## Examples

| Example | Topics |
|---|---|
| [Basic operations](examples/basic.cpp) | Typed reads, fallbacks, batch updates and manual saving |
| [Configuration sources](examples/sources.cpp) | Defaults, file layers, environment variables and encoding |
| [Observing changes](examples/observe.cpp) | Validation, subscriptions and file watching |
| [Struct serialization](examples/serialization.cpp) | Reading and writing custom C++ types |

All examples are built and run by the test suite. The quick-start snippet above is checked against its compiled source.

## Build and test

From the repository root, run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure --parallel
```

On Windows, use a Visual Studio developer shell. See [build options](docs/BUILD.md) for sanitizers and coverage, or [contributing](CONTRIBUTING.md) for the development workflow.

## License

[MIT](LICENSE.txt).
