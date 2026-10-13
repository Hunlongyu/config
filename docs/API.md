# API reference — 0.2.2

[中文](API.zh-CN.md) · [Home](../README.md) · [Migration](MIGRATION.md)

Include `<config/config.hpp>`. Optional helpers have their own headers: `<config/watcher.hpp>`, `<config/paths.hpp>`.

## Store

`Store(std::filesystem::path file, StoreOptions options = {})` opens one writable file. Relative paths resolve against the current working directory at construction. Options are copied and remain immutable. Store is neither copyable nor movable. Share its ownership explicitly when needed.

Missing writable files start empty without creating directories. Existing unreadable, malformed or non-object files throw. Duplicate active owners of an equivalent file throw `in_use`; no persistent registry retains old stores.

| Operation | Contract |
|---|---|
| `get<T>(pointer)` | Read effective value; missing → `missing`, conversion failure → `type`. |
| `get<T>(pointer, fallback)` | Return the fallback only for absence; wrong types and invalid paths still throw. |
| `get<T>(pointer, factory)` | Invoke a zero-argument factory only for absence and return its result. |
| `find<T>(pointer)` | Return `std::optional<T>`; only absence produces `nullopt`. Wrong types and invalid paths throw. |
| `contains(pointer)` | Test the same effective view used by get, find and snapshot. |
| `snapshot(View::effective)` | Detached root object; includes all layers. |
| `snapshot(View::writable)` | Detached writable layer only. An unsupported View throws `type`. |
| `set(pointer, json, Encoding::none)` | Set writable value; custom types use nlohmann serialization. Encoding requires a string. |
| `erase(pointer)` | Return true after removing a writable value; lower layers may become visible again. Return false without committing when absent from the writable layer. |
| `update(fn)` | Edit a copy of the writable root, validate and commit once. Root must stay an object. |
| `merge(json)` | Recursive object merge into writable data; arrays/scalars replace entire values. |
| `merge_file(path)` | Decode with the common codec, then merge; missing files are errors. |
| `save()` | Serialize the current writable layer; failure throws and retains the old disk file. |
| `reload()` | Rebuild all sources and refresh environment. Explicitly discards unsaved writable changes on success. |
| `subscribe(callback)` | Return a move-only, nodiscard Subscription. Callback receives `const Change&`. |
| `path()` | Resolved absolute filesystem path. |
| `revision()` | Monotonic commit generation; save may advance it without an effective-value event. |
| `dirty()` | Writable data has an unpersisted mutation; save/reload clear it. |

Keys are nonempty JSON Pointers beginning with `/`. `~1` escapes slash and `~0` escapes tilde. The empty pointer is rejected; use snapshot/update for root operations. Empty property segments remain valid. Array addressing follows nlohmann JSON Pointer semantics.

### Typed reads and fallbacks

Specify an owning, unqualified value type `T` explicitly for `get` and `find`. References, raw pointers and range views (including `std::string_view` and `std::span`) are rejected at compile time. Container elements and custom deserializers must also own their returned data; the library cannot inspect ownership inside arbitrary types. Use `std::string` for text. Read and status-query results are `[[nodiscard]]`.

A fallback value must be implicitly convertible to `T`. Boolean fallbacks must be bool; numeric fallbacks must be arithmetic values other than bool. These rules also apply to factory results, so a wrong-signature callback cannot become a numeric or boolean fallback through an implicit conversion. A zero-argument callable is always treated as a factory, even when it is also convertible to `T`. For example, `get<bool>("/enabled", [] { return false; })` returns false on absence; supplying a callback that requires an int argument is a compile error.

Built-in numeric `T` uses checked conversion: integers reject overflow, negative-to-unsigned conversion and fractional values; integral floating-point values such as `42.0` can become integers. Floating-point results allow normal rounding but reject nonfinite values, overflow and underflow to zero. Booleans and numbers do not convert to one another. The same checks apply to numeric fallback values and factory results, only when used. A rejected conversion throws `ErrorCode::type`. Containers, enums and custom structs continue to use nlohmann deserialization; validate their member conversions in the deserializer when checked numeric behavior is required.

Each read uses one effective snapshot. Existing values, including JSON null, never invoke the factory. Null is converted to `T` normally: `get<json>` preserves it, while `get<int>` reports a type error. Wrong types and invalid paths do not fall back.

`find<T>` checks presence and converts within one snapshot. Use it to distinguish absence from a stored value equal to a default, without a separate `contains`/`get` sequence. `find<json>` returns an engaged optional for JSON null; `find<int>` throws for null. It never writes, changes revision/dirty state or emits events.

The factory runs synchronously on the calling thread, at most once and outside internal locks. It is neither stored nor retried if another thread changes the key. Its exceptions propagate unchanged and do not go through `on_callback_error`. The library does not save the fallback, change the revision or emit an event. A factory may read the Store again; any explicit mutations made by user code retain their usual effects.

Passing a value evaluates that argument before the call. Use a factory to defer expensive work until a key is missing. Factories support mutable, move-only and reference-qualified callable objects without `std::function` conversion.

### Batch updates

`erase` reports whether a value existed in the writable layer and was successfully removed. A value supplied only by defaults, a file layer or the environment does not count. A missing writable key skips validation, persistence and publication; revision, dirty state and events stay unchanged. A present key follows the normal transaction rules, and failures throw rather than returning false. Concurrent mutations may produce `conflict`; erase never retries against newer state implicitly.

`update(fn)` also accepts a callable directly, including move-only captures. It invokes `fn(json&)` once on a candidate outside internal locks and does not retain the callable. Its own exceptions propagate. Configuration errors from JSON path operations become `config::Error`. A concurrent commit may reject the batch with `conflict`; the library never reruns user code implicitly.

## StoreOptions

| Field | Default / meaning |
|---|---|
| `save` | `SaveMode::automatic`; alternatively `manual`. |
| `format` | `Format::pretty`; alternatively `compact`. |
| `defaults` | Empty JSON object; lowest priority, never saved. |
| `layers` | Ordered `Layer{file, optional=false}` entries; later layers win. |
| `env_prefix` | Empty disables prefix mapping. Suffix ASCII letters become lowercase, underscores become path separators. |
| `env_bindings` | Map of JSON Pointer → environment name; explicit bindings override prefix mapping. |
| `validate` | Optional `void(const json&)`; validates the candidate effective view at construction and on every mutation/reload. |
| `on_callback_error` | Optional `void(std::exception_ptr)`; observes subscription failures. |
| `backend` | Optional shared Backend; defaults to the atomic filesystem backend. |
| `legacy_encoding` | False. Enable only while importing encoded 0.1 files. |

Optional layers skip only missing files; invalid or unreadable layers always fail. Environment values parse as JSON when possible, otherwise as strings. They are sampled at construction/reload, not per get. Callers must synchronize process-wide environment modifications; the library cannot lock external environment writers.

Reads resolve defaults → layers → writable values → environment. Saving never copies lower layers or environment into writable data. An environment override can mask a successful writable set; events describe effective changes.

Validation runs outside locks and may read committed state. A validator should be pure: mutating or saving the same Store can make its transaction conflict. Thrown validation exceptions become `validation`.

## Errors and persistence

`Error` derives from `std::runtime_error`; `code()`, `file()` and `key()` provide structured context. Key-related read/conversion/path errors include the requested JSON Pointer (or the original invalid input); errors without a specific key return an empty string. Codes: `missing`, `type`, `path`, `read`, `parse`, `write`, `validation`, `conflict`, `in_use`, `format`.

Automatic mutations prepare → validate → persist → commit → notify. Read, parse, validation, serialization, conflict or write failure cannot publish the candidate or its events. Manual mutations commit memory and mark dirty; a later save failure retains those explicitly unsaved edits. Allocation failures and exceptions from user callbacks retain their standard C++ exception semantics.

Files use the `config/v1` format introduced in 0.2:
```json
{"$config":"config/v1","values":{"port":8080},"encoding":{}}
```
Encoding rules map canonical pointers to integers 0–5 for none/base64/hex/rot13/reverse/combined. Business keys, including `__obfuscate_meta__` and `$config`, live inside values and round-trip unchanged. Plain object imports are supported; a top-level `$config` beginning with `config/` is reserved for the file-format envelope. Unknown format versions and malformed rules fail.

Encoding is reversible representation, not secret protection. Replacing a subtree removes its rules; erasing an array entry shifts surviving rules. Arbitrary update/merge drops rules for changed values; call set with an encoding to assign new ones.

## Subscription and Change

`Change{revision, before, after}` describes one committed effective-view batch. Equal effective views produce no event; save alone produces none. Use `json::diff(before, after)` when per-key changes are needed.

Keep the returned Subscription. Destruction or `disconnect()` prevents future callback starts; it does not wait for a callback already running. `connected()` becomes false when disconnected or when the Store dies. Moving over an existing handle disconnects its former subscription.

A default-constructed handle is disconnected. Active handles are created by `Store::subscribe`; the internal slot constructor is private.

Events dispatch in commit order outside state/writer locks. Callbacks may read, mutate, subscribe or disconnect. Reentrant events queue behind the current batch. If another thread is already dispatching, a mutation can return before its callbacks run. Callbacks execute on the dispatching thread; marshal to a UI thread yourself. A newly subscribed callback does not receive already queued events. Exceptions are isolated and forwarded to on_callback_error; exceptions from that handler are ignored.

## Watcher and paths

`Watcher(Store&, interval=1s, on_error={})` polls writable and layer file contents, so equal timestamps do not hide edits. A failed candidate remains unacknowledged and is retried. Successful reload clears `last_error()`; the error handler runs outside locks. Environment-only changes require explicit reload. Unsaved manual edits block watcher reloads with conflict.

`stop()` interrupts the polling wait. It cannot interrupt a user callback or filesystem operation already running. It is idempotent; after cancellation one stop caller owns the join while concurrent callers may return earlier. Destruction from its own callback detaches the worker after cancellation; worker state is independent of the Watcher object. A Watcher may outlive its Store and does not permanently retain it.

`user_config_directory(application)` returns a path without creating it. Application is one nonempty directory name. Windows uses LocalAppData, macOS uses ~/Library/Application Support, and Linux uses absolute XDG_CONFIG_HOME or ~/.config. Missing platform directories are errors, not silent fallback to cwd.

## Version

Version metadata is available separately through `<config/version.h>`. `version()` returns a `constexpr std::string_view` with static storage lifetime and is `noexcept`; explicitly construct `std::string` when an owning copy is needed.

## Backend

`Backend::read(path)` returns optional bytes (nullopt only for a missing file). `write(path, bytes)` atomically replaces a complete file or throws Error while retaining the old file. Implementations must tolerate concurrent reads, translate storage errors, and must not reenter Store operations from write. This single extension boundary supports deterministic fault/concurrency tests.
