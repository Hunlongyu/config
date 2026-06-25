# API Reference

All symbols live in the `config` namespace (`#include <config/store.hpp>`).

Keys and JSON Pointer paths are interchangeable throughout the API. A bare key
`"server/host"` is treated identically to the pointer `"/server/host"` — the
library prepends the leading slash automatically when the key does not start
with one.

---

## Types & Enums

### `Path`

Resolves where a configuration file lives on disk.

| Value | Description |
|---|---|
| `Absolute` | Use the path string as-is. |
| `Relative` | Resolve relative to the current working directory. |
| `AppData` | Resolve under the platform app-data directory (`%APPDATA%` on Windows, `~/.config` on Linux/macOS). |

---

### `SaveStrategy`

Controls when in-memory changes are flushed to disk.

| Value | Description |
|---|---|
| `Auto` | Persist after every `set`, `remove`, or `clear` call. |
| `Manual` | Persist only when `save()` is called explicitly. |

---

### `MissingKeyPolicy`

Controls what `get<T>(key)` does when the key is absent and no `default_value`
argument was supplied.

| Value | Description |
|---|---|
| `DefaultValue` | Return `T{}` (zero/empty value for the type). |
| `ThrowException` | Throw `std::runtime_error` with file/line diagnostics. |

---

### `JsonFormat`

Output format for `save()` and `dump()`.

| Value | Description |
|---|---|
| `Pretty` | 4-space indented, human-readable. |
| `Compact` | Minified, no whitespace. |

---

### `Encoding`

Per-key encoding applied transparently on write and reversed on read. Only
`std::string` values are encoded; non-string values stored with a non-`None`
encoding are written and read back unchanged.

| Value | Description |
|---|---|
| `None` | Plaintext (default). |
| `Base64` | Base64 encoding. |
| `Hex` | Hexadecimal string encoding. |
| `ROT13` | ROT13 substitution cipher. |
| `Reverse` | String reversal. |
| `Combined` | Base64 followed by Reverse. |

---

### `StoreOptions`

Options bundle for the two-argument `ConfigStore` constructor. All fields have
defaults so only the fields you care about need to be set.

```cpp
struct StoreOptions {
    Path         path_type  = Path::Relative;
    SaveStrategy save       = SaveStrategy::Auto;
    MissingKeyPolicy on_missing = MissingKeyPolicy::DefaultValue;
    JsonFormat   format     = JsonFormat::Pretty;
    std::string  env_prefix;  // empty = no prefix-based env overrides
};
```

When `env_prefix` is non-empty, every environment variable whose name starts
with that prefix is mapped to a config key at load/reload time. The prefix is
stripped, the remaining name is lowercased, and underscores become `/`
separators (e.g., `APP_SERVER_PORT` with prefix `APP_` → key `server/port`).

---

### `SaveError`

```cpp
class SaveError : public std::runtime_error { /* ... */ };
```

Thrown by `set`, `remove`, `clear`, `merge`, `merge_file`, `load_layered`, and
`get_or_set` when `SaveStrategy::Auto` is active and the disk write fails.
Inherits `std::runtime_error`; the message describes which operation failed.

---

### `Connection`

RAII handle returned by `connect`, `on_change`, and `on_any_change`. The
listener is automatically removed when the `Connection` goes out of scope.

`Connection` is move-only (copy is deleted).

| Member | Description |
|---|---|
| `void disconnect()` | Disconnect immediately, before destruction. |
| `size_t id() const` | Internal listener ID. |
| `bool connected() const` | `true` while the listener is active. |

A `Connection` **must not outlive the `ConfigStore`** it was created from.

---

## ConfigStore — Construction

### `ConfigStore(path, type, save_strategy, missing_key_policy)`

```cpp
ConfigStore(const std::string &path,
            Path             type                = Path::Relative,
            SaveStrategy     save_strategy       = SaveStrategy::Auto,
            MissingKeyPolicy missing_key_policy  = MissingKeyPolicy::DefaultValue);
```

Constructs a store and immediately loads data from disk (creating an empty
store if the file does not exist).

| Param | Description |
|---|---|
| `path` | File path to the JSON config file. |
| `type` | How to resolve `path`. |
| `save_strategy` | Auto-save behavior. |
| `missing_key_policy` | Behavior on missing key in `get<T>(key)`. |

---

### `ConfigStore(path, opts)`

```cpp
explicit ConfigStore(const std::string &path, StoreOptions opts);
```

Options-bundle variant. Prefer this when configuring more than one or two
non-default settings. `opts.env_prefix` is only honoured by this constructor.

`ConfigStore` is non-copyable and non-movable.

---

## ConfigStore — Configuration

### `set_save_strategy`

```cpp
void set_save_strategy(SaveStrategy strategy);
```

Changes the save strategy at runtime. Thread-safe.

---

### `get_save_strategy`

```cpp
[[nodiscard]] SaveStrategy get_save_strategy() const;
```

Returns the current save strategy.

---

### `set_missing_key_policy`

```cpp
void set_missing_key_policy(MissingKeyPolicy policy);
```

Changes how `get<T>(key)` behaves when a key is absent.

---

### `missing_key_policy`

```cpp
[[nodiscard]] MissingKeyPolicy missing_key_policy() const;
```

Returns the current missing-key policy.

---

### `set_format`

```cpp
void set_format(JsonFormat format);
```

Sets the JSON output format used by subsequent `save()` calls.

---

### `get_format`

```cpp
[[nodiscard]] JsonFormat get_format() const;
```

Returns the current JSON output format.

---

## ConfigStore — Data Access

### `get` (with default)

```cpp
template <typename T>
T get(std::string_view key, const T &default_value) const;
```

Retrieves the value at `key`, returning `default_value` if the key is absent
or the stored value cannot be converted to `T`. The `MissingKeyPolicy` is
**ignored** by this overload — `default_value` is always the fallback.

An empty `key` (`""`) deserializes the entire root JSON object into `T`.

**Returns:** The stored value, a default from the defaults layer (see
`set_default`), or `default_value`.

---

### `get` (policy-driven)

```cpp
template <typename T>
T get(std::string_view key,
      std::source_location location = std::source_location::current()) const;
```

Retrieves the value at `key`. Missing-key behavior is determined by
`MissingKeyPolicy`:

- `DefaultValue` — returns `T{}`.
- `ThrowException` — throws `std::runtime_error` with the key name and
  source location embedded in the message.

An empty `key` deserializes the root; failure follows the same policy rules.

**Returns:** The stored value or a default from the defaults layer.
**Throws:** `std::runtime_error` when policy is `ThrowException` and key is absent.

---

### `set`

```cpp
template <typename T>
void set(std::string_view key, const T &value,
         Encoding encoding = Encoding::None,
         std::source_location location = std::source_location::current());
```

Writes `value` to `key`. If `encoding` is not `None` and `value` is a
`std::string`, the stored bytes are encoded; the original string is restored
transparently on `get`. Calling `set("", obj)` replaces the entire root; `obj`
must be a JSON-object-compatible type.

Triggers listeners registered for `key` (and any ancestor wildcard listeners)
after the write.

**Throws:**
- `std::invalid_argument` — key is `""` and `value` is not a JSON object type.
- `std::runtime_error` — an in-memory path conflict (e.g., trying to set a key
  beneath a scalar node).
- `SaveError` — auto-save is active and the disk write fails.

---

### `contains`

```cpp
[[nodiscard]] bool contains(std::string_view key) const;
```

Returns `true` if `key` exists in the live config data. An empty key returns
`true` when the root object is non-empty.

---

### `remove`

```cpp
void remove(std::string_view key);
```

Removes the key and its value. No-op if the key does not exist.

**Throws:** `SaveError` — auto-save is active and the disk write fails.

---

### `clear`

```cpp
void clear();
```

Removes all keys and resets the obfuscation map. The defaults layer (see
`set_default`) is **not** cleared.

**Throws:** `SaveError` — auto-save is active and the disk write fails.

---

## ConfigStore — Advanced Get

### `get_or_set`

```cpp
template <typename T>
T get_or_set(std::string_view key, const T &default_value);
```

Atomic read-or-initialize. Returns the existing value if `key` is present and
readable as `T`; otherwise writes `default_value` to `key` and returns it.
Fires listeners and auto-saves when initialization occurs.

`key` must be non-empty.

**Returns:** The existing or newly-written value.
**Throws:**
- `std::invalid_argument` — key is empty.
- `SaveError` — initialization triggered auto-save and the write failed.

---

### `get_all`

```cpp
template <typename T>
[[nodiscard]] std::unordered_map<std::string, T>
get_all(std::string_view prefix = "") const;
```

Returns all immediate children of the node at `prefix` as a typed map. Each
child value that cannot be converted to `T` is silently skipped.

An empty `prefix` operates on the root object.

**Returns:** Map from child key name to deserialized value.

---

### `get_root`

```cpp
template <typename T>
T get_root(const T &default_value) const;

template <typename T>
T get_root(std::source_location location = std::source_location::current()) const;
```

Convenience wrappers for `get<T>("")`. The first overload always falls back to
`default_value`; the second follows `MissingKeyPolicy`.

---

### `set_root`

```cpp
template <typename T>
void set_root(const T &value);
```

Replaces the entire root JSON object. Equivalent to `set("", value)`. `value`
must be a JSON-object-compatible type.

**Throws:** same as `set`.

---

### `sub`

```cpp
[[nodiscard]] json sub(std::string_view prefix) const;
```

Returns a point-in-time deep copy of the subtree rooted at `prefix`. Changes
to the store after this call are not reflected in the returned object.

An empty `prefix` returns a copy of the entire root.

**Returns:** The JSON value at `prefix`, or `json::object()` if not found.

---

### `bind_env`

```cpp
void bind_env(std::string_view key, std::string_view env_var);
```

Registers an explicit mapping from the environment variable `env_var` to the
config key `key`. This is independent of `StoreOptions::env_prefix` — both
mechanisms can coexist.

The binding is applied the next time the store loads or `reload()` is called.
Calling `bind_env` does **not** immediately apply the value; follow it with
`reload()` to force a re-read.

The env var value is JSON-parsed when possible, otherwise stored as a plain
string.

---

## ConfigStore — Keys

### `keys`

```cpp
std::vector<std::string> keys(std::string_view prefix = "") const;
```

Returns the names of immediate child keys at the node identified by `prefix`.
An empty `prefix` returns top-level keys. Only one level deep — use `all_keys`
for full recursion.

**Returns:** Vector of child key names (not full paths).

---

### `children`

```cpp
std::vector<std::string> children(std::string_view prefix = "") const;
```

Alias for `keys()`. Identical behavior.

---

### `all_keys`

```cpp
[[nodiscard]] std::vector<std::string> all_keys(std::string_view prefix = "") const;
```

Recursively collects every leaf path in the subtree rooted at `prefix`. Arrays
are treated as leaves — their elements are not recursed into.

An empty `prefix` starts from the root.

**Returns:** Unordered vector of full leaf paths (e.g., `"server/host"`).

```cpp
// Given: { "server": { "host": "localhost", "port": 8080 }, "debug": true }
store.keys()     // → ["server", "debug"]
store.all_keys() // → ["server/host", "server/port", "debug"]
```

---

## ConfigStore — Defaults

The defaults layer provides fallback values that `get` consults when a key is
absent from the live config. Defaults survive `reload()` and `clear()`, and are
never written to disk.

### `set_default`

```cpp
template <typename T>
void set_default(std::string_view key, const T &value);
```

Registers a fallback value for `key`. The defaults layer has lower priority
than live data: `get()` only reads this value when the key is absent from
`data_`.

`key` must be non-empty.

**Throws:** `std::invalid_argument` — key is empty.

---

### `clear_defaults`

```cpp
void clear_defaults();
```

Removes all registered default values.

---

## ConfigStore — Persistence

### `save` (current format)

```cpp
[[nodiscard]] bool save() const;
```

Flushes the current in-memory config to disk using the format set by
`set_format()` (default: `Pretty`). Creates intermediate directories as needed.
Values with `Encoding` other than `None` are encrypted in the file; decryption
happens transparently at load time.

**Returns:** `true` on success, `false` on any I/O error.

---

### `save` (explicit format)

```cpp
[[nodiscard]] bool save(JsonFormat format) const;
```

Same as `save()` but uses the specified `format` for this call only; does not
change the stored format setting.

**Returns:** `true` on success, `false` on any I/O error.

---

### `reload`

```cpp
void reload();
```

Discards the current in-memory state and reloads from disk. Env overrides
(prefix-based and `bind_env` bindings) are re-applied after loading. If a
validator is registered it is called on the freshly loaded data; if it throws,
the previous data is restored and the exception propagates.

---

### `merge`

```cpp
void merge(const json &overlay);
```

Deep-merges `overlay` into the current config. Nested objects are merged
recursively; scalar and array values are overwritten by `overlay`.

**Throws:**
- `std::invalid_argument` — `overlay` is not a JSON object.
- `SaveError` — auto-save is active and the disk write fails.

---

### `merge_file`

```cpp
void merge_file(const std::string &path, Path type = Path::Relative);
```

Loads a JSON file from `path` and deep-merges it into the current config.

| Param | Description |
|---|---|
| `path` | Path to the JSON file to load. |
| `type` | How to resolve `path`. |

**Throws:**
- `std::runtime_error` — file does not exist.
- `SaveError` — auto-save is active and the disk write fails.

---

### `load_layered`

```cpp
void load_layered(const std::vector<std::string> &paths,
                  Path type = Path::Relative);
```

Merges multiple JSON files in order. Files are processed left-to-right so
later entries take precedence over earlier ones. Non-existent files and files
that fail to parse are silently skipped. Env overrides are re-applied after all
layers have been merged.

| Param | Description |
|---|---|
| `paths` | Ordered list of file paths to load. |
| `type` | Path resolution strategy applied to each entry. |

**Throws:** `SaveError` — auto-save is active and the final disk write fails.

```cpp
// Load base config, then environment overlay, then secrets overlay
store.load_layered({"config.json", "config.production.json", "secrets.json"});
```

---

## ConfigStore — Listeners

Listeners fire synchronously in the calling thread after every successful
`set`, `remove`, or `clear` operation. Exceptions thrown inside a callback are
silently swallowed to protect other listeners and the caller.

### `connect`

```cpp
Connection connect(const std::string &key,
                   const std::function<void(const json &)> &callback);
```

Registers `callback` to fire whenever `key` or any of its children change.
The callback receives the current value of `key` at the time of notification
(not the intermediate writes).

**Returns:** A `Connection` that auto-disconnects on destruction.

---

### `on_change`

```cpp
template <typename T>
Connection on_change(const std::string &key,
                     std::function<void(const T &)> callback);
```

Typed variant of `connect`. The JSON value is automatically deserialized to
`T` before the callback is invoked. Deserialization failures are silently
ignored.

**Returns:** A `Connection` that auto-disconnects on destruction.

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

Wildcard listener — fires on every `set`, `remove`, or `clear` call regardless
of the key. The callback receives the new value of the key that changed.

Implemented as `connect("", callback)` — an empty key is the wildcard
sentinel.

**Returns:** A `Connection` that auto-disconnects on destruction.

---

### `disconnect`

```cpp
void disconnect(size_t connection_id);
```

Removes the listener with the given ID. Prefer letting `Connection` destruct
naturally; call this only when you need to disconnect without destroying the
`Connection` object.

---

## ConfigStore — File Watcher

### `start_watch`

```cpp
void start_watch(std::chrono::milliseconds interval = std::chrono::milliseconds{1000});
```

Spawns a background thread that polls the config file's modification time
every `interval`. When a change is detected, `reload()` is called
automatically. Calling `start_watch` while already watching has no effect.

| Param | Description |
|---|---|
| `interval` | Polling interval. Default: 1000 ms. |

---

### `stop_watch`

```cpp
void stop_watch();
```

Signals the watcher thread to stop and blocks until it exits. Called
automatically by the destructor.

---

## ConfigStore — Validation

### `set_validator`

```cpp
void set_validator(std::function<void(const json &)> validator);
```

Registers a callback invoked at the end of every `reload()` call, after env
overrides are applied, but outside the internal mutex (so the validator may
safely call back into the store). Throw from the validator to reject the loaded
data — `reload()` will restore the previous state and re-throw.

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

Removes the registered validator. Subsequent `reload()` calls will not perform
any validation.

---

## ConfigStore — Utilities

### `dump`

```cpp
[[nodiscard]] std::string dump(JsonFormat format = JsonFormat::Pretty) const;
```

Serializes the live in-memory config to a JSON string. Returns the decrypted
representation — the same data that `get()` reads. Nothing is written to disk.

**Returns:** JSON string in the requested format.

---

### `path`

```cpp
std::filesystem::path path() const;
```

Returns the resolved absolute path to the config file as a
`std::filesystem::path`.

---

### `get_store_path`

```cpp
std::string get_store_path() const;
```

Returns the resolved absolute path to the config file as a `std::string`.

---

### `path_type`

```cpp
Path path_type() const;
```

Returns the `Path` enum value that was used to construct this store.

---

### `get_save_strategy`

Returns the current `SaveStrategy`. See [Configuration](#configstore--configuration).

---

### `missing_key_policy`

Returns the current `MissingKeyPolicy`. See [Configuration](#configstore--configuration).

---

### `get_format`

Returns the current `JsonFormat`. See [Configuration](#configstore--configuration).

---

## Global Functions

All free functions in the `config` namespace operate on the **default store**,
which is a singleton `ConfigStore` backed by `"config.json"` (relative path).
They are thin wrappers over the corresponding `ConfigStore` member functions.

### Registry

#### `get_store`

```cpp
ConfigStore &get_store(std::string_view path,
                       Path             type                = Path::Relative,
                       SaveStrategy     save_strategy       = SaveStrategy::Auto,
                       MissingKeyPolicy missing_key_policy  = MissingKeyPolicy::DefaultValue);

ConfigStore &get_store(std::string_view path, StoreOptions opts);
```

Retrieves a named store from the global registry, creating it on first access.
Stores are keyed by path string. If the store already exists with a different
`Path` type, a `std::logic_error` is thrown.

**Returns:** Reference to the (possibly new) `ConfigStore`.

---

#### `get_default_store`

```cpp
ConfigStore &get_default_store();
```

Returns the default store (`"config.json"`, relative path). All the free
functions below delegate to this store.

---

#### `release_store`

```cpp
bool release_store(std::string_view path);
```

Removes the store entry for `path` from the global registry. Existing
references to the store remain valid until they go out of scope.

**Returns:** `true` if the entry was present and removed, `false` otherwise.

---

#### `release_all_stores`

```cpp
void release_all_stores();
```

Clears the entire global registry. Existing references remain valid.

---

### Data Access

```cpp
template <typename T> T    get(std::string_view key, const T &default_value);
template <typename T> T    get(std::string_view key);
template <typename T> void set(std::string_view key, const T &value,
                                Encoding encoding = Encoding::None);
template <typename T> T    get_or_set(std::string_view key, const T &default_value);
void                       remove(std::string_view key);
[[nodiscard]] bool         contains(std::string_view key);
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Advanced Get

```cpp
template <typename T>
[[nodiscard]] std::unordered_map<std::string, T> get_all(std::string_view prefix = "");

template <typename T> T    get_root(const T &default_value);
template <typename T> T    get_root();
template <typename T> void set_root(const T &value);

[[nodiscard]] ConfigStore::json sub(std::string_view prefix);
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Keys

```cpp
std::vector<std::string>         keys(std::string_view prefix = "");
std::vector<std::string>         children(std::string_view prefix = "");
[[nodiscard]] std::vector<std::string> all_keys(std::string_view prefix = "");
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Defaults

```cpp
template <typename T> void set_default(std::string_view key, const T &value);
void                       clear_defaults();
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Persistence

```cpp
[[nodiscard]] bool save();
[[nodiscard]] bool save(JsonFormat format);
void               reload();
void               merge(const nlohmann::json &overlay);
void               merge_file(const std::string &path, Path type = Path::Relative);
void               load_layered(const std::vector<std::string> &paths,
                                Path type = Path::Relative);
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Listeners

```cpp
Connection connect(const std::string &key,
                   const std::function<void(const ConfigStore::json &)> &callback);

template <typename T>
Connection on_change(const std::string &key, std::function<void(const T &)> callback);

Connection on_any_change(const std::function<void(const ConfigStore::json &)> &callback);

template <typename T>
Connection on_any_change(std::function<void(const T &)> callback);
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### File Watcher

```cpp
void start_watch(std::chrono::milliseconds interval = std::chrono::milliseconds{1000});
void stop_watch();
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Validation

```cpp
void set_validator(std::function<void(const ConfigStore::json &)> validator);
void clear_validator();
```

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Configuration & Utilities

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

Equivalent to the same-named `ConfigStore` members on the default store.

---

### Environment Bindings

```cpp
void bind_env(std::string_view key, std::string_view env_var);
```

Equivalent to `ConfigStore::bind_env` on the default store.
