# API Reference

This document provides a comprehensive reference for the `config` library API.

## Namespace: `config`

All library components are located within the `config` namespace.

---

### Enumerations

#### `Path`
Defines path types for configuration files.
| Enum Value | Description |
| :--- | :--- |
| `Absolute` | Absolute file path. |
| `Relative` | Relative path to the current working directory. |
| `AppData` | System-specific application data directory (e.g., `%APPDATA%` on Windows, `~/.config` on Linux). |

#### `JsonFormat`
Defines JSON output formats.
| Enum Value | Description |
| :--- | :--- |
| `Pretty` | Indented output (4 spaces) for readability. |
| `Compact` | Minified output without whitespace. |

#### `Obfuscate`
Defines available obfuscation methods.
| Enum Value | Description |
| :--- | :--- |
| `None` | No obfuscation (plaintext). |
| `Base64` | Base64 encoding. |
| `Hex` | Hexadecimal string encoding. |
| `ROT13` | ROT13 substitution cipher. |
| `Reverse` | String reversal. |
| `Combined` | Combined strategy (Base64 + Reverse). |

#### `SaveStrategy`
Defines when configuration changes are saved to disk.
| Enum Value | Description |
| :--- | :--- |
| `Auto` | Automatically save to disk after every `set`, `remove`, or `clear` operation. |
| `Manual` | Only save to disk when `save()` is explicitly called. |

#### `GetStrategy`
Defines behavior when a key is missing during retrieval.
| Enum Value | Description |
| :--- | :--- |
| `DefaultValue` | Return a type-specific default value (or user-provided default) if key is missing. |
| `ThrowException` | Throw a `std::runtime_error` if key is missing. |

---

### Class: `ConfigStore`

Thread-safe configuration store managing JSON data persistence and retrieval.

#### Constructor
```cpp
ConfigStore(const std::string &path,
            const Path type = Path::Relative,
            const SaveStrategy save_strategy = SaveStrategy::Auto,
            const GetStrategy get_strategy = GetStrategy::DefaultValue)
```
*   **path**: File path for the configuration file.
*   **type**: Strategy for resolving the file path.
*   **save_strategy**: Initial save strategy.
*   **get_strategy**: Initial retrieval strategy.

#### Core Methods

##### `get`
```cpp
template <typename T> T get(const std::string &key, const T &default_value) const;
template <typename T> T get(const std::string &key) const;
```
Retrieves a value from the configuration.
*   **key**: The configuration key or JSON Pointer path (e.g., "section/key").
*   **default_value**: (Optional) Value to return if key is missing.
*   **Returns**: The retrieved value.
*   **Throws**: `std::runtime_error` if key is missing and strategy is `ThrowException`.

##### `set`
```cpp
template <typename T> bool set(const std::string &key, const T &value, const Obfuscate obf = Obfuscate::None);
```
Sets a value in the configuration.
*   **key**: The configuration key or JSON Pointer path.
*   **value**: The value to store.
*   **obf**: Obfuscation method to apply.
*   **Returns**: `true` if operation succeeded (including auto-save), `false` if auto-save failed.
*   **Throws**: `std::runtime_error` for logic errors (e.g., path conflicts).

##### `remove`
```cpp
bool remove(const std::string &key);
```
Removes a key and its value.
*   **Returns**: `true` if operation succeeded (including auto-save), `false` if auto-save failed.

##### `contains`
```cpp
bool contains(const std::string &key) const;
```
Checks if a key exists.

##### `save`
```cpp
bool save() const;
bool save(JsonFormat format) const;
```
Saves the configuration to disk.
*   **Returns**: `true` if successful, `false` otherwise.

##### `load` / `reload`
```cpp
void reload();
```
Reloads configuration from disk, discarding memory changes.

##### `clear`
```cpp
bool clear();
```
Clears all data.
*   **Returns**: `true` if operation succeeded (including auto-save), `false` if auto-save failed.

#### Listener Methods

##### `connect`
```cpp
size_t connect(const std::string &key, const std::function<void(const nlohmann::json &)> &callback);
```
Connects a listener to a key.

##### `disconnect`
```cpp
void disconnect(size_t connection_id);
```
Disconnects a listener.

---

### Global Convenience Functions

These functions operate on a default `ConfigStore` instance (singleton per file path, default "config.json").

| Function | Description |
| :--- | :--- |
| `ConfigStore &get_store(path, ...)` | Retrieves or creates a `ConfigStore` instance. |
| `get<T>(key, [default])` | Gets a value from the default store. |
| `set(key, value, [obf])` | Sets a value in the default store. |
| `remove(key)` | Removes a key from the default store. |
| `contains(key)` | Checks for key existence. |
| `save([format])` | Saves the default store. |
| `load()` / `reload()` | Reloads the default store. |
| `clear()` | Clears the default store. |
| `set_save_strategy(s)` | Sets save strategy. |
| `set_get_strategy(s)` | Sets get strategy. |
| `set_format(f)` | Sets JSON format. |
