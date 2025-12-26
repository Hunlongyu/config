# Config Library

A modern C++20 configuration management library built on [nlohmann/json](https://github.com/nlohmann/json). It provides a thread-safe, easy-to-use API with support for various storage strategies, data obfuscation, and change listeners.

[中文文档](README_CN.md) | [API Reference](docs/API_Reference.md)

## ✨ Features

- **Header-only**: Just include the header file to use.
- **Easy to Use**: Provides global `get`/`set` interfaces like a map, ready out of the box.
- **JSON Pointer**: Supports accessing deep nested data using `/` separated paths (e.g., `user/profile/name`).
- **Multiple Storage Strategies**:
  - **Auto Save (`Auto`)**: Writes to file immediately after modification.
  - **Manual Save (`Manual`)**: Writes only when `save()` is explicitly called.
- **Flexible Path Management**:
  - **Relative**: Relative to the application execution directory.
  - **Absolute**: Specify the full file path.
  - **System Path (`AppData`)**: Automatically adapts to Windows (%LOCALAPPDATA%), macOS (~/Library), Linux (~/.config).
- **Data Obfuscation**: Built-in Base64, Hex, ROT13, Reverse, and Combined strategies to protect sensitive configuration.
- **Change Listeners**: Support registering callbacks to listen for configuration item changes in real-time.
- **Thread Safe**: Global interfaces and `ConfigStore` instances are guaranteed to be thread-safe.

## 📦 Integration

### Requirements
- C++20 Compiler
- CMake 3.28+ (if using CMake integration)

### Method 1: CMake FetchContent (Recommended)

Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
  config
  GIT_REPOSITORY https://github.com/Hunlongyu/config.git
  GIT_TAG main  # Recommended to specify a specific tag
)
FetchContent_MakeAvailable(config)

target_link_libraries(your_target PRIVATE config::config)
```

### Method 2: Manual Integration

1. Ensure your project includes `nlohmann/json`.
2. Copy the `include/config` directory to your project's include path.
3. Add `#include <config/config.hpp>` in your code.

## 🚀 Quick Start

```cpp
#include <config/config.hpp>
#include <iostream>

int main() {
    // 1. Set config (Auto-saved to config.json in run directory)
    config::set("server/host", "127.0.0.1");
    config::set("server/port", 8080);
    config::set("app/debug", true);

    // 2. Get config (Supports default values)
    std::string host = config::get<std::string>("server/host", "localhost");
    int port = config::get<int>("server/port", 80);

    std::cout << "Server running at " << host << ":" << port << std::endl;

    return 0;
}
```

## 📖 Documentation

For more detailed usage instructions, please refer to the following documents:

*   **[API Reference](docs/API_Reference.md)**: Detailed API specification, class methods, and enumerations.
*   **[Usage Examples](docs/Examples.md)**: Code snippets demonstrating common use cases.
*   **[Build & Install Guide](docs/Build_and_Install.md)**: Instructions on how to build examples, run tests, and integrate the library.

## 📄 License

MIT License
