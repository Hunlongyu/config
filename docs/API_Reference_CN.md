# API 参考文档

所有符号均位于 `config` 命名空间中（`#include <config/store.hpp>`）。

键与 JSON Pointer 路径在整个 API 中可互换使用。裸键 `"server/host"` 与指针 `"/server/host"` 的处理方式完全相同——当键不以斜杠开头时，库会自动补充前导斜杠。

---

## 类型与枚举

### `Path`

指定配置文件在磁盘上的路径解析方式。

| 枚举值 | 描述 |
|---|---|
| `Absolute` | 直接使用路径字符串，不做任何处理。 |
| `Relative` | 相对于当前工作目录解析。 |
| `AppData` | 解析到平台应用数据目录（Windows 上为 `%APPDATA%`，Linux/macOS 上为 `~/.config`）。 |

---

### `SaveStrategy`

控制内存中的变更何时写入磁盘。

| 枚举值 | 描述 |
|---|---|
| `Auto` | 每次调用 `set`、`remove` 或 `clear` 后自动持久化。 |
| `Manual` | 仅在显式调用 `save()` 时持久化。 |

---

### `MissingKeyPolicy`

控制当键不存在且未提供 `default_value` 参数时，`get<T>(key)` 的行为。

| 枚举值 | 描述 |
|---|---|
| `DefaultValue` | 返回 `T{}`（该类型的零值或空值）。 |
| `ThrowException` | 抛出 `std::runtime_error`，并在消息中附带文件名和行号诊断信息。 |

---

### `JsonFormat`

`save()` 和 `dump()` 的输出格式。

| 枚举值 | 描述 |
|---|---|
| `Pretty` | 4 空格缩进，便于人工阅读。 |
| `Compact` | 压缩格式，无空白字符。 |

---

### `Encoding`

按键应用的 encoding，写入时透明编码，读取时自动还原。仅对 `std::string` 类型的值进行编码；以非 `None` encoding 存储的非字符串值，写入和读取时保持不变。

| 枚举值 | 描述 |
|---|---|
| `None` | 明文（默认值）。 |
| `Base64` | Base64 encoding。 |
| `Hex` | 十六进制字符串 encoding。 |
| `ROT13` | ROT13 替换加密。 |
| `Reverse` | 字符串反转。 |
| `Combined` | 先 Base64 encoding，再反转。 |

---

### `StoreOptions`

双参数 `ConfigStore` 构造函数的选项包。所有字段均有默认值，仅需设置关心的字段。

```cpp
struct StoreOptions {
    Path         path_type  = Path::Relative;
    SaveStrategy save       = SaveStrategy::Auto;
    MissingKeyPolicy on_missing = MissingKeyPolicy::DefaultValue;
    JsonFormat   format     = JsonFormat::Pretty;
    std::string  env_prefix;  // empty = no prefix-based env overrides
};
```

当 `env_prefix` 非空时，名称以该 prefix 开头的所有环境变量会在加载/重新加载时映射到配置键。映射规则：去掉 prefix，剩余部分转为小写，下划线替换为 `/` 分隔符（例如，prefix 为 `APP_` 时，`APP_SERVER_PORT` → 键 `server/port`）。

---

### `SaveError`

```cpp
class SaveError : public std::runtime_error { /* ... */ };
```

当 `SaveStrategy::Auto` 生效且磁盘写入失败时，由 `set`、`remove`、`clear`、`merge`、`merge_file`、`load_layered` 和 `get_or_set` 抛出。继承自 `std::runtime_error`，消息中描述了哪个操作失败。

---

### `Connection`

由 `connect`、`on_change` 和 `on_any_change` 返回的 RAII 句柄。当 `Connection` 离开作用域时，监听器自动移除。

`Connection` 仅可移动（不可复制）。

| 成员 | 描述 |
|---|---|
| `void disconnect()` | 在析构前立即断开连接。 |
| `size_t id() const` | 内部监听器 ID。 |
| `bool connected() const` | 监听器处于活跃状态时返回 `true`。 |

`Connection` **的生命周期不得超过创建它的 `ConfigStore`**。

---

## ConfigStore — 构造

### `ConfigStore(path, type, save_strategy, missing_key_policy)`

```cpp
ConfigStore(const std::string &path,
            Path             type                = Path::Relative,
            SaveStrategy     save_strategy       = SaveStrategy::Auto,
            MissingKeyPolicy missing_key_policy  = MissingKeyPolicy::DefaultValue);
```

构造一个存储并立即从磁盘加载数据（若文件不存在则创建空存储）。

| 参数 | 描述 |
|---|---|
| `path` | JSON 配置文件的路径。 |
| `type` | `path` 的解析方式。 |
| `save_strategy` | 自动保存行为。 |
| `missing_key_policy` | `get<T>(key)` 遇到缺失键时的行为。 |

---

### `ConfigStore(path, opts)`

```cpp
explicit ConfigStore(const std::string &path, StoreOptions opts);
```

选项包形式的构造函数。当需要配置两个以上非默认设置时，建议优先使用此形式。`opts.env_prefix` 仅在此构造函数中生效。

`ConfigStore` 不可复制，也不可移动。

---

## ConfigStore — 配置

### `set_save_strategy`

```cpp
void set_save_strategy(SaveStrategy strategy);
```

在运行时更改保存策略。thread-safe。

---

### `get_save_strategy`

```cpp
[[nodiscard]] SaveStrategy get_save_strategy() const;
```

返回当前的保存策略。

---

### `set_missing_key_policy`

```cpp
void set_missing_key_policy(MissingKeyPolicy policy);
```

更改 `get<T>(key)` 在键缺失时的行为。

---

### `missing_key_policy`

```cpp
[[nodiscard]] MissingKeyPolicy missing_key_policy() const;
```

返回当前的缺失键策略。

---

### `set_format`

```cpp
void set_format(JsonFormat format);
```

设置后续 `save()` 调用所使用的 JSON 输出格式。

---

### `get_format`

```cpp
[[nodiscard]] JsonFormat get_format() const;
```

返回当前的 JSON 输出格式。

---

## ConfigStore — 数据访问

### `get`（带默认值）

```cpp
template <typename T>
T get(std::string_view key, const T &default_value) const;
```

获取 `key` 处的值；若键不存在或存储的值无法转换为 `T`，则返回 `default_value`。此重载**忽略** `MissingKeyPolicy`——始终以 `default_value` 作为 fallback。

`key` 为空（`""`）时，将整个根 JSON 对象反序列化为 `T`。

**返回：** 存储的值、默认层中的默认值（参见 `set_default`），或 `default_value`。

---

### `get`（策略驱动）

```cpp
template <typename T>
T get(std::string_view key,
      std::source_location location = std::source_location::current()) const;
```

获取 `key` 处的值。缺失键的行为由 `MissingKeyPolicy` 决定：

- `DefaultValue` — 返回 `T{}`。
- `ThrowException` — 抛出 `std::runtime_error`，消息中嵌入键名和源码位置。

`key` 为空时反序列化根对象；失败时遵循相同的策略规则。

**返回：** 存储的值或默认层中的默认值。
**抛出：** 当策略为 `ThrowException` 且键缺失时，抛出 `std::runtime_error`。

---

### `set`

```cpp
template <typename T>
void set(std::string_view key, const T &value,
         Encoding encoding = Encoding::None,
         std::source_location location = std::source_location::current());
```

将 `value` 写入 `key`。若 `encoding` 不为 `None` 且 `value` 为 `std::string`，存储的字节将被编码；`get` 时会透明地还原原始字符串。调用 `set("", obj)` 将替换整个根对象；`obj` 必须为兼容 JSON 对象的类型。

写入完成后，触发已注册到 `key` 的监听器（以及任何祖先 wildcard 监听器）。

**抛出：**
- `std::invalid_argument` — 键为 `""` 且 `value` 不是 JSON 对象类型。
- `std::runtime_error` — 内存中存在路径冲突（例如，尝试在标量节点下设置键）。
- `SaveError` — 自动保存已启用且磁盘写入失败。

---

### `contains`

```cpp
[[nodiscard]] bool contains(std::string_view key) const;
```

若 `key` 在当前配置数据中存在，返回 `true`。空键在根对象非空时返回 `true`。

---

### `remove`

```cpp
void remove(std::string_view key);
```

移除该键及其值。若键不存在则为空操作。

**抛出：** `SaveError` — 自动保存已启用且磁盘写入失败。

---

### `clear`

```cpp
void clear();
```

移除所有键并重置 obfuscation 映射表。默认层（参见 `set_default`）**不会**被清除。

**抛出：** `SaveError` — 自动保存已启用且磁盘写入失败。

---

## ConfigStore — 高级获取

### `get_or_set`

```cpp
template <typename T>
T get_or_set(std::string_view key, const T &default_value);
```

原子性的"读取或初始化"操作。若 `key` 存在且可读取为 `T`，返回已有值；否则将 `default_value` 写入 `key` 并返回它。初始化时会触发监听器并自动保存。

`key` 不得为空。

**返回：** 已有值或新写入的值。
**抛出：**
- `std::invalid_argument` — 键为空。
- `SaveError` — 初始化触发自动保存且写入失败。

---

### `get_all`

```cpp
template <typename T>
[[nodiscard]] std::unordered_map<std::string, T>
get_all(std::string_view prefix = "") const;
```

以类型化映射的形式返回 `prefix` 节点的所有直接子节点。无法转换为 `T` 的子节点将被静默跳过。

`prefix` 为空时操作根对象。

**返回：** 子键名到反序列化值的映射。

---

### `get_root`

```cpp
template <typename T>
T get_root(const T &default_value) const;

template <typename T>
T get_root(std::source_location location = std::source_location::current()) const;
```

`get<T>("")` 的便捷包装。第一个重载始终回退到 `default_value`；第二个遵循 `MissingKeyPolicy`。

---

### `set_root`

```cpp
template <typename T>
void set_root(const T &value);
```

替换整个根 JSON 对象，等价于 `set("", value)`。`value` 必须为兼容 JSON 对象的类型。

**抛出：** 同 `set`。

---

### `sub`

```cpp
[[nodiscard]] json sub(std::string_view prefix) const;
```

返回以 `prefix` 为根的子树的深度拷贝（时间点快照）。此调用之后对存储的修改不会反映到返回的对象中。

`prefix` 为空时返回整个根对象的副本。

**返回：** `prefix` 处的 JSON 值；若未找到则返回 `json::object()`。

---

### `bind_env`

```cpp
void bind_env(std::string_view key, std::string_view env_var);
```

注册从环境变量 `env_var` 到配置键 `key` 的显式映射关系。此机制与 `StoreOptions::env_prefix` 相互独立，可同时使用。

绑定在下次存储加载或调用 `reload()` 时生效。调用 `bind_env` **不会**立即应用值；如需强制重新读取，请在调用后执行 `reload()`。

环境变量的值会尽可能解析为 JSON，否则以纯字符串存储。

---

## ConfigStore — 键

### `keys`

```cpp
std::vector<std::string> keys(std::string_view prefix = "") const;
```

返回 `prefix` 节点的直接子键名称列表。`prefix` 为空时返回顶层键。仅查询一层——如需完整递归，请使用 `all_keys`。

**返回：** 子键名称向量（非完整路径）。

---

### `children`

```cpp
std::vector<std::string> children(std::string_view prefix = "") const;
```

`keys()` 的别名，行为完全相同。

---

### `all_keys`

```cpp
[[nodiscard]] std::vector<std::string> all_keys(std::string_view prefix = "") const;
```

递归收集以 `prefix` 为根的子树中所有叶路径。数组被视为叶节点，不会递归其元素。

`prefix` 为空时从根开始。

**返回：** 完整叶路径的无序向量（例如 `"server/host"`）。

```cpp
// Given: { "server": { "host": "localhost", "port": 8080 }, "debug": true }
store.keys()     // → ["server", "debug"]
store.all_keys() // → ["server/host", "server/port", "debug"]
```

---

## ConfigStore — 默认值

默认层为 `get` 在键不存在于当前配置时提供 fallback 值。默认值在 `reload()` 和 `clear()` 后仍然保留，且永远不会写入磁盘。

### `set_default`

```cpp
template <typename T>
void set_default(std::string_view key, const T &value);
```

为 `key` 注册一个 fallback 值。默认层的优先级低于实时数据：仅当 `data_` 中不存在该键时，`get()` 才会读取此值。

`key` 不得为空。

**抛出：** `std::invalid_argument` — 键为空。

---

### `clear_defaults`

```cpp
void clear_defaults();
```

移除所有已注册的默认值。

---

## ConfigStore — 持久化

### `save`（当前格式）

```cpp
[[nodiscard]] bool save() const;
```

使用 `set_format()` 设置的格式（默认为 `Pretty`）将当前内存中的配置写入磁盘，必要时自动创建中间目录。encoding 不为 `None` 的值在文件中以加密形式存储，加载时透明解密。

**返回：** 成功返回 `true`，发生任何 I/O 错误时返回 `false`。

---

### `save`（指定格式）

```cpp
[[nodiscard]] bool save(JsonFormat format) const;
```

与 `save()` 相同，但仅本次调用使用指定的 `format`，不会修改已存储的格式设置。

**返回：** 成功返回 `true`，发生任何 I/O 错误时返回 `false`。

---

### `reload`

```cpp
void reload();
```

丢弃当前内存状态并从磁盘重新加载。加载完成后重新应用环境变量覆盖（基于 prefix 的方式和 `bind_env` 绑定）。若已注册 validator，则对新加载的数据调用它；若 validator 抛出异常，则恢复之前的数据并继续传播该异常。

---

### `merge`

```cpp
void merge(const json &overlay);
```

将 `overlay` 深度合并到当前配置中。嵌套对象递归合并；标量值和数组值由 `overlay` 中的值覆盖。

**抛出：**
- `std::invalid_argument` — `overlay` 不是 JSON 对象。
- `SaveError` — 自动保存已启用且磁盘写入失败。

---

### `merge_file`

```cpp
void merge_file(const std::string &path, Path type = Path::Relative);
```

从 `path` 加载 JSON 文件并深度合并到当前配置中。

| 参数 | 描述 |
|---|---|
| `path` | 要加载的 JSON 文件路径。 |
| `type` | `path` 的解析方式。 |

**抛出：**
- `std::runtime_error` — 文件不存在。
- `SaveError` — 自动保存已启用且磁盘写入失败。

---

### `load_layered`

```cpp
void load_layered(const std::vector<std::string> &paths,
                  Path type = Path::Relative);
```

按顺序合并多个 JSON 文件。文件从左到右依次处理，后面的条目优先级高于前面的。不存在的文件和解析失败的文件会被静默跳过。所有层合并完成后重新应用环境变量覆盖。

| 参数 | 描述 |
|---|---|
| `paths` | 要加载的文件路径有序列表。 |
| `type` | 应用于每个条目的路径解析策略。 |

**抛出：** `SaveError` — 自动保存已启用且最终磁盘写入失败。

```cpp
// Load base config, then environment overlay, then secrets overlay
store.load_layered({"config.json", "config.production.json", "secrets.json"});
```

---

## ConfigStore — 监听器

监听器在每次成功的 `set`、`remove` 或 `clear` 操作后，在调用线程中同步触发。callback 内部抛出的异常将被静默吞掉，以保护其他监听器和调用方不受影响。

### `connect`

```cpp
Connection connect(const std::string &key,
                   const std::function<void(const json &)> &callback);
```

注册 `callback`，在 `key` 或其任意子节点发生变化时触发。callback 接收到的是通知时刻 `key` 的当前值（而非中间写入值）。

**返回：** 析构时自动断开连接的 `Connection`。

---

### `on_change`

```cpp
template <typename T>
Connection on_change(const std::string &key,
                     std::function<void(const T &)> callback);
```

`connect` 的类型化版本。在调用 callback 之前，JSON 值会自动反序列化为 `T`。反序列化失败时静默忽略。

**返回：** 析构时自动断开连接的 `Connection`。

```cpp
auto conn = store.on_change<int>("server/port", [](int port) {
    std::cout << "Port changed to " << port << "\n";
});
```

---

### `on_any_change`

```cpp
Connection on_any_change(const std::function<void(const json &)> &callback);

template <typename T>
Connection on_any_change(std::function<void(const T &)> callback);
```

Wildcard 监听器——无论哪个键发生变化，每次 `set`、`remove` 或 `clear` 调用都会触发。callback 接收发生变化的键的新值。

内部实现为 `connect("", callback)`——空键是 wildcard 哨兵值。

**返回：** 析构时自动断开连接的 `Connection`。

---

### `disconnect`

```cpp
void disconnect(size_t connection_id);
```

移除指定 ID 的监听器。建议优先通过 `Connection` 析构自然断开；仅在需要在不销毁 `Connection` 对象的情况下断开连接时才调用此方法。

---

## ConfigStore — 文件监视器

### `start_watch`

```cpp
void start_watch(std::chrono::milliseconds interval = std::chrono::milliseconds{1000});
```

启动一个后台线程，每隔 `interval` 轮询一次配置文件的修改时间。检测到变化时自动调用 `reload()`。在已监视的情况下再次调用 `start_watch` 无任何效果。

| 参数 | 描述 |
|---|---|
| `interval` | 轮询间隔，默认值：1000 毫秒。 |

---

### `stop_watch`

```cpp
void stop_watch();
```

通知监视器线程停止并阻塞直至其退出。析构函数会自动调用此方法。

---

## ConfigStore — 验证

### `set_validator`

```cpp
void set_validator(std::function<void(const json &)> validator);
```

注册一个 callback，在每次 `reload()` 调用结束时、应用环境变量覆盖之后调用，但在内部互斥锁之外执行（因此 validator 可以安全地回调存储）。在 validator 中抛出异常可拒绝加载的数据——`reload()` 将恢复之前的状态并重新抛出异常。

```cpp
store.set_validator([](const config::ConfigStore::json &data) {
    if (!data.contains("server"))
        throw std::runtime_error("Missing required section: server");
});
```

---

### `clear_validator`

```cpp
void clear_validator();
```

移除已注册的 validator。后续的 `reload()` 调用将不再执行任何验证。

---

## ConfigStore — 工具方法

### `dump`

```cpp
[[nodiscard]] std::string dump(JsonFormat format = JsonFormat::Pretty) const;
```

将当前内存中的配置序列化为 JSON 字符串。返回解密后的表示——与 `get()` 读取的数据相同。不会向磁盘写入任何内容。

**返回：** 指定格式的 JSON 字符串。

---

### `path`

```cpp
std::filesystem::path path() const;
```

以 `std::filesystem::path` 形式返回配置文件的已解析绝对路径。

---

### `get_store_path`

```cpp
std::string get_store_path() const;
```

以 `std::string` 形式返回配置文件的已解析绝对路径。

---

### `path_type`

```cpp
Path path_type() const;
```

返回构造此存储时所使用的 `Path` 枚举值。

---

### `get_save_strategy`

返回当前的 `SaveStrategy`。参见[配置](#configstore--配置)。

---

### `missing_key_policy`

返回当前的 `MissingKeyPolicy`。参见[配置](#configstore--配置)。

---

### `get_format`

返回当前的 `JsonFormat`。参见[配置](#configstore--配置)。

---

## 全局函数

`config` 命名空间中的所有自由函数均操作**默认存储**，即一个以 `"config.json"`（相对路径）为后端的单例 `ConfigStore`。它们是对应 `ConfigStore` 成员函数的薄包装。

### 注册表

#### `get_store`

```cpp
ConfigStore &get_store(std::string_view path,
                       Path             type                = Path::Relative,
                       SaveStrategy     save_strategy       = SaveStrategy::Auto,
                       MissingKeyPolicy missing_key_policy  = MissingKeyPolicy::DefaultValue);

ConfigStore &get_store(std::string_view path, StoreOptions opts);
```

从全局注册表中获取具名存储，首次访问时创建。存储以路径字符串为键。若已存在同路径但 `Path` 类型不同的存储，则抛出 `std::logic_error`。

**返回：** 对（可能新创建的）`ConfigStore` 的引用。

---

#### `get_default_store`

```cpp
ConfigStore &get_default_store();
```

返回默认存储（`"config.json"`，相对路径）。下面所有自由函数均委托给此存储。

---

#### `release_store`

```cpp
bool release_store(std::string_view path);
```

从全局注册表中移除 `path` 对应的存储条目。已有的存储引用在离开作用域前仍然有效。

**返回：** 若条目存在并已移除，返回 `true`；否则返回 `false`。

---

#### `release_all_stores`

```cpp
void release_all_stores();
```

清空整个全局注册表。已有引用仍然有效。

---

### 数据访问

```cpp
template <typename T> T    get(std::string_view key, const T &default_value);
template <typename T> T    get(std::string_view key);
template <typename T> void set(std::string_view key, const T &value,
                                Encoding encoding = Encoding::None);
template <typename T> T    get_or_set(std::string_view key, const T &default_value);
void                       remove(std::string_view key);
[[nodiscard]] bool         contains(std::string_view key);
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 高级获取

```cpp
template <typename T>
[[nodiscard]] std::unordered_map<std::string, T> get_all(std::string_view prefix = "");

template <typename T> T    get_root(const T &default_value);
template <typename T> T    get_root();
template <typename T> void set_root(const T &value);

[[nodiscard]] ConfigStore::json sub(std::string_view prefix);
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 键

```cpp
std::vector<std::string>         keys(std::string_view prefix = "");
std::vector<std::string>         children(std::string_view prefix = "");
[[nodiscard]] std::vector<std::string> all_keys(std::string_view prefix = "");
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 默认值

```cpp
template <typename T> void set_default(std::string_view key, const T &value);
void                       clear_defaults();
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 持久化

```cpp
[[nodiscard]] bool save();
[[nodiscard]] bool save(JsonFormat format);
void               reload();
void               merge(const nlohmann::json &overlay);
void               merge_file(const std::string &path, Path type = Path::Relative);
void               load_layered(const std::vector<std::string> &paths,
                                Path type = Path::Relative);
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 监听器

```cpp
Connection connect(const std::string &key,
                   const std::function<void(const ConfigStore::json &)> &callback);

template <typename T>
Connection on_change(const std::string &key, std::function<void(const T &)> callback);

Connection on_any_change(const std::function<void(const ConfigStore::json &)> &callback);

template <typename T>
Connection on_any_change(std::function<void(const T &)> callback);
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 文件监视器

```cpp
void start_watch(std::chrono::milliseconds interval = std::chrono::milliseconds{1000});
void stop_watch();
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 验证

```cpp
void set_validator(std::function<void(const ConfigStore::json &)> validator);
void clear_validator();
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 配置与工具方法

```cpp
void             set_save_strategy(SaveStrategy strategy);
SaveStrategy     get_save_strategy();
void             set_missing_key_policy(MissingKeyPolicy policy);
MissingKeyPolicy missing_key_policy();
void             set_format(JsonFormat format);
JsonFormat       get_format();
std::string      get_store_path();
std::filesystem::path path();
[[nodiscard]] std::string dump(JsonFormat format = JsonFormat::Pretty);
```

等价于默认存储上同名的 `ConfigStore` 成员函数。

---

### 环境变量绑定

```cpp
void bind_env(std::string_view key, std::string_view env_var);
```

等价于默认存储上 `ConfigStore::bind_env`。
