# Usage Examples

This document provides detailed examples covering various features of the library. You can find the full source code for these examples in the [examples/](../examples/) directory.

## 1. Basic Usage
[Source: examples/01_basic_usage.cpp](../examples/01_basic_usage.cpp)

Shows how to perform basic `get` and `set` operations with type safety and default values.

```cpp
// Set values
config::set("app/name", "MyApp");
config::set("app/version", 1);

// Get values
std::string name = config::get<std::string>("app/name");
int version = config::get<int>("app/version", 0); // Default to 0 if missing
```

## 2. Save Strategies
[Source: examples/02_save_strategies.cpp](../examples/02_save_strategies.cpp)

Demonstrates the difference between `Auto` (default) and `Manual` save modes.

```cpp
// Switch to Manual mode for batch updates
config::set_save_strategy(config::SaveStrategy::Manual);

for (int i = 0; i < 100; ++i) {
    config::set("data/" + std::to_string(i), i);
}

config::save(); // Write to disk only once
```

## 3. Get Strategies
[Source: examples/03_get_strategies.cpp](../examples/03_get_strategies.cpp)

Shows how to handle missing keys: return default value vs. throw exception.

```cpp
config::set_get_strategy(config::GetStrategy::ThrowException);

try {
    auto val = config::get<int>("missing_key");
} catch (const std::exception& e) {
    std::cerr << "Caught expected error: " << e.what() << std::endl;
}
```

## 4. Data Obfuscation
[Source: examples/04_obfuscation.cpp](../examples/04_obfuscation.cpp)

Protect sensitive data like passwords using built-in obfuscation.

```cpp
// Store encrypted
config::set("db/password", "SecretPass", config::Obfuscate::Base64);

// Read decrypted (transparently)
std::string pass = config::get<std::string>("db/password");
```

## 5. Path Resolution
[Source: examples/05_path_strategies.cpp](../examples/05_path_strategies.cpp)

Save config files to system-standard locations (AppData/Home).

```cpp
// Automatically resolves to %APPDATA%/MyApp/user.json on Windows
auto& store = config::get_store("user.json", config::Path::AppData);
```

## 6. Listeners
[Source: examples/06_listeners.cpp](../examples/06_listeners.cpp)

React to configuration changes in real-time.

```cpp
config::connect("theme", [](const nlohmann::json& val) {
    std::cout << "Theme changed to: " << val << std::endl;
});

config::set("theme", "Dark"); // Triggers callback
```

## 7. Global Functions
[Source: examples/07_global_functions.cpp](../examples/07_global_functions.cpp)

Demonstrates the convenient global API wrappers.
