# API 参考文档

本文档提供了 `config` 库 API 的详细参考说明。

## 命名空间: `config`

所有库组件均位于 `config` 命名空间内。

---

### 枚举类型 (Enumerations)

#### `Path`
定义配置文件的路径解析类型。
| 枚举值 | 描述 |
| :--- | :--- |
| `Absolute` | 绝对文件路径。 |
| `Relative` | 相对于当前工作目录的路径。 |
| `AppData` | 系统特定的应用程序数据目录 (如 Windows 上的 `%APPDATA%`, Linux 上的 `~/.config`)。 |

#### `JsonFormat`
定义 JSON 输出格式。
| 枚举值 | 描述 |
| :--- | :--- |
| `Pretty` | 格式化输出（缩进 4 个空格），便于阅读。 |
| `Compact` | 紧凑输出，无空白字符，适用于节省空间。 |

#### `Obfuscate`
定义可用的混淆方法。
| 枚举值 | 描述 |
| :--- | :--- |
| `None` | 不混淆 (明文)。 |
| `Base64` | Base64 编码。 |
| `Hex` | 十六进制字符串编码。 |
| `ROT13` | ROT13 替换加密。 |
| `Reverse` | 字符串反转。 |
| `Combined` | 组合策略 (先 Base64 编码，再反转)。 |

#### `SaveStrategy`
定义配置变更时的保存策略。
| 枚举值 | 描述 |
| :--- | :--- |
| `Auto` | 自动保存：在每次 `set`, `remove` 或 `clear` 操作后立即写入磁盘。 |
| `Manual` | 手动保存：只有在显式调用 `save()` 时才写入磁盘。 |

#### `GetStrategy`
定义获取不存在的键时的行为。
| 枚举值 | 描述 |
| :--- | :--- |
| `DefaultValue` | 如果键缺失，返回类型特定的默认值（或用户提供的默认值）。 |
| `ThrowException` | 如果键缺失，抛出 `std::runtime_error` 异常。 |

---

### 类: `ConfigStore`

管理 JSON 数据持久化和读取的线程安全配置存储类。

#### 构造函数
```cpp
ConfigStore(const std::string &path,
            const Path type = Path::Relative,
            const SaveStrategy save_strategy = SaveStrategy::Auto,
            const GetStrategy get_strategy = GetStrategy::DefaultValue)
```
*   **path**: 配置文件路径。
*   **type**: 路径解析策略。
*   **save_strategy**: 初始保存策略。
*   **get_strategy**: 初始读取策略。

#### 核心方法

##### `get`
```cpp
template <typename T> T get(const std::string &key, const T &default_value) const;
template <typename T> T get(const std::string &key) const;
```
从配置中获取值。
*   **key**: 配置键或 JSON Pointer 路径 (例如 "section/key")。
*   **default_value**: (可选) 如果键缺失时返回的默认值。
*   **返回**: 获取到的值。
*   **异常**: 如果键缺失且策略为 `ThrowException`，抛出 `std::runtime_error`。

##### `set`
```cpp
template <typename T> bool set(const std::string &key, const T &value, const Obfuscate obf = Obfuscate::None);
```
设置配置项的值。
*   **key**: 配置键或 JSON Pointer 路径。
*   **value**: 要存储的值。
*   **obf**: 要应用的混淆方法。
*   **返回**: `true` 表示操作成功（包含自动保存），`false` 表示自动保存失败。
*   **异常**: `std::runtime_error` 用于逻辑错误（例如路径类型冲突）。

##### `remove`
```cpp
bool remove(const std::string &key);
```
移除一个键及其值。
*   **返回**: `true` 表示操作成功（包含自动保存），`false` 表示自动保存失败。

##### `contains`
```cpp
bool contains(const std::string &key) const;
```
检查键是否存在。

##### `save`
```cpp
bool save() const;
bool save(JsonFormat format) const;
```
将当前配置保存到磁盘。
*   **返回**: `true` 表示保存成功，`false` 表示失败（如 IO 错误）。

##### `load` / `reload`
```cpp
void reload();
```
从磁盘重新加载配置，丢弃当前内存中的更改。

##### `clear`
```cpp
bool clear();
```
清空所有数据。
*   **返回**: `true` 表示操作成功（包含自动保存），`false` 表示自动保存失败。

#### 监听器方法

##### `connect`
```cpp
size_t connect(const std::string &key, const std::function<void(const nlohmann::json &)> &callback);
```
连接一个监听器到指定键，当该键值变化时触发回调。

##### `disconnect`
```cpp
void disconnect(size_t connection_id);
```
断开监听器连接。

---

### 全局便捷函数

这些函数操作默认的 `ConfigStore` 实例（每个文件路径单例，默认为 "config.json"）。

| 函数 | 描述 |
| :--- | :--- |
| `ConfigStore &get_store(path, ...)` | 获取或创建 `ConfigStore` 实例。 |
| `get<T>(key, [default])` | 从默认存储中获取值。 |
| `set(key, value, [obf])` | 设置默认存储中的值。 |
| `remove(key)` | 从默认存储中移除键。 |
| `contains(key)` | 检查默认存储中是否存在键。 |
| `save([format])` | 保存默认存储到磁盘。 |
| `load()` / `reload()` | 重新加载默认存储。 |
| `clear()` | 清空默认存储。 |
| `set_save_strategy(s)` | 设置保存策略。 |
| `set_get_strategy(s)` | 设置读取策略。 |
| `set_format(f)` | 设置 JSON 格式。 |
