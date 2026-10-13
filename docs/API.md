# API reference — 0.2

[中文](API.zh-CN.md) · [Home](../README.md) · [Migration](MIGRATION.md)

Include `<config/config.hpp>`. Optional helpers have their own headers: `<config/watcher.hpp>`, `<config/paths.hpp>`.

## Store

`Store(std::filesystem::path file, StoreOptions options = {})` opens one writable file. Relative paths resolve against the current working directory at construction. Options are copied and remain immutable. Store is neither copyable nor movable. Share its ownership explicitly when needed.

Missing writable files start empty without creating directories. Existing unreadable, malformed or non-object files throw. Duplicate active owners of an equivalent file throw `in_use`; no persistent registry retains old stores.

| Operation | Contract |
|---|---|
| `get<T>(pointer)` | Read effective value; missing → `missing`, conversion failure → `type`. |
| `get_or<T>(pointer, fallback)` | Fallback only for absence, never for wrong types or invalid paths. |
| `contains(pointer)` | Test the same effective view used by get and snapshot. |
| `snapshot(View::effective)` | Detached root object; includes all layers. |
| `snapshot(View::writable)` | Detached writable layer only. |
| `set(pointer, json, Encoding::none)` | Set writable value; custom types use nlohmann serialization. Encoding requires a string. |
| `erase(pointer)` | Remove writable value; lower layers may become visible again. Missing values are harmless. |
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

The batch callback executes once on a candidate outside internal locks. Its own exceptions propagate. Configuration errors from JSON path operations become `config::Error`. A concurrent commit may reject the batch with `conflict`; the library never reruns user code implicitly.

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

`Error` derives from `std::runtime_error`; `code()` and `file()` provide structured context. Codes: `missing`, `type`, `path`, `read`, `parse`, `write`, `validation`, `conflict`, `in_use`, `format`.

Automatic mutations prepare → validate → persist → commit → notify. Read, parse, validation, serialization, conflict or write failure cannot publish the candidate or its events. Manual mutations commit memory and mark dirty; a later save failure retains those explicitly unsaved edits. Allocation failures and exceptions from user callbacks retain their standard C++ exception semantics.

Files written by 0.2 use:
```json
{"$config":"config/v1","values":{"port":8080},"encoding":{}}
```
Encoding rules map canonical pointers to integers 0–5 for none/base64/hex/rot13/reverse/combined. Business keys, including `__obfuscate_meta__` and `$config`, live inside values and round-trip unchanged. Plain object imports are supported; a top-level `$config` beginning with `config/` is reserved for the file-format envelope. Unknown format versions and malformed rules fail.

Encoding is reversible representation, not secret protection. Replacing a subtree removes its rules; erasing an array entry shifts surviving rules. Arbitrary update/merge drops rules for changed values; call set with an encoding to assign new ones.

## Subscription and Change

`Change{revision, before, after}` describes one committed effective-view batch. Equal effective views produce no event; save alone produces none. Use `json::diff(before, after)` when per-key changes are needed.

Keep the returned Subscription. Destruction or `disconnect()` prevents future callback starts; it does not wait for a callback already running. `connected()` becomes false when disconnected or when the Store dies. Moving over an existing handle disconnects its former subscription.

Events dispatch in commit order outside state/writer locks. Callbacks may read, mutate, subscribe or disconnect. Reentrant events queue behind the current batch. If another thread is already dispatching, a mutation can return before its callbacks run. Callbacks execute on the dispatching thread; marshal to a UI thread yourself. A newly subscribed callback does not receive already queued events. Exceptions are isolated and forwarded to on_callback_error; exceptions from that handler are ignored.

## Watcher and paths

`Watcher(Store&, interval=1s, on_error={})` polls writable and layer file contents, so equal timestamps do not hide edits. A failed candidate remains unacknowledged and is retried. Successful reload clears `last_error()`; the error handler runs outside locks. Environment-only changes require explicit reload. Unsaved manual edits block watcher reloads with conflict.

`stop()` interrupts the polling wait. It cannot interrupt a user callback or filesystem operation already running. It is idempotent; after cancellation one stop caller owns the join while concurrent callers may return earlier. Destruction from its own callback detaches the worker after cancellation; worker state is independent of the Watcher object. A Watcher may outlive its Store and does not permanently retain it.

`user_config_directory(application)` returns a path without creating it. Application is one nonempty directory name. Windows uses LocalAppData, macOS uses ~/Library/Application Support, and Linux uses absolute XDG_CONFIG_HOME or ~/.config. Missing platform directories are errors, not silent fallback to cwd.

## Backend

`Backend::read(path)` returns optional bytes (nullopt only for a missing file). `write(path, bytes)` atomically replaces a complete file or throws Error while retaining the old file. Implementations must tolerate concurrent reads, translate storage errors, and must not reenter Store operations from write. This single extension boundary supports deterministic fault/concurrency tests.
