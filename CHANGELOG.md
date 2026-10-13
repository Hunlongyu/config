# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.2.0] - 2026-10-13

### Breaking changes

- Replace ConfigStore and global wrappers with an explicitly owned Store, immutable StoreOptions and JSON Pointer keys.
- Replace duplicate retrieval, path and subscription APIs with get/get_or, snapshot/update, path and Subscription.
- Use structured Error codes; save throws on failure, and concurrent candidate conflicts are explicit.
- Write a versioned config/v1 envelope. Plain objects import directly; old encoded files require explicit legacy_encoding import.

### Architecture and fixes

- Separate state transactions, codec, storage backend, subscriptions, platform paths and optional Watcher.
- Validate every candidate, persist automatic mutations before publication, and preserve valid state after failed reloads.
- Keep defaults, file layers, writable values and environment separate; save only writable data.
- Use one decoder for construction, reload, layers and file merges; preserve business metadata-like fields.
- Enforce one active file owner across equivalent paths without retaining old instances.
- Dispatch ordered batch events outside locks; subscriptions may outlive Store, and watcher failures retry without new timestamps.
- Reuse parent JSON targets, prevent Windows macro leakage, install the license and fix localized MSVC dependency tracking.

### Verification and documentation

- Consolidate duplicate tests into behavior suites with isolated temporary directories.
- Add deterministic transaction/save regressions and a mutation guard that must reject removal of the save lock.
- Test independent headers, multiple translation units, installed packages, parent projects, manual integration and compiled examples.
- Synchronize English/Chinese API, architecture, build and migration documents; validate snippets against their source examples.
- Run platform and sanitizer jobs in CI; publish release assets only after all validation jobs succeed.
- Use atomic GCC coverage counters so concurrent tests do not corrupt profiling data.

## 0.1 development history

### Fixed (0.1.4)

- Listener dispatch snapshots callbacks safely during reentrant or concurrent registration and disconnection
- Registry removal keeps existing references valid; `get_store_handle()` adds shared ownership for stores requiring earlier reclamation
- Concurrent saves take snapshots in persistence order; serialization and file replacement failures preserve the previous configuration
- Windows environment enumeration compiles with `UNICODE`; POSIX enumeration resolves the global `environ` symbol, and environment tests use platform-specific setters
- Reload validates unpublished candidates and detects concurrent changes instead of exposing rejected data or rolling back newer writes
- Reload rebuilds encoding metadata; equivalent JSON Pointer keys share one rule, and removing or replacing subtrees clears affected rules
- Reverse encoding preserves UTF-8 characters
- Removal, root replacement, and clearing notify listeners; array removal shifts remaining elements and their encoding rules
- Recursive key enumeration escapes `/` and `~` in JSON Pointer segments
- Installing the fetched JSON dependency includes its headers and CMake package for standalone consumers
- Save strategy and output format reads synchronize with their setters

### Tests

- Added regression coverage for lifetime, callback reentrancy, concurrency, validation, failed writes, encoding, paths, Unicode builds, and isolated installation consumption

### Added

- `StoreOptions` aggregate constructor — named-parameter bundle (`path_type`, `save`, `on_missing`, `format`, `env_prefix`) as an alternative to the positional four-argument `ConfigStore` constructor
- `SaveError` exception class — thrown by `set()`, `remove()`, and `clear()` when an auto-save disk write fails, replacing the former `bool` return value
- `Connection` RAII handle — returned by all listener registration calls; auto-disconnects on scope exit, eliminating manual `disconnect()` calls for the common case
- `on_change<T>(key, callback)` — typed change listener that deserializes the value before invoking the callback
- `on_any_change(callback)` / `on_any_change<T>(callback)` — wildcard listeners that fire on every mutation regardless of key
- `get_or_set<T>(key, default_value)` — atomic read-or-initialize: returns the existing value if present, otherwise writes the default and returns it
- `get_all<T>(prefix)` — returns a typed `unordered_map<string, T>` of all immediate children under a prefix
- `all_keys(prefix)` — recursive leaf-key enumeration (returns every terminal path under the prefix)
- `keys(prefix)` / `children(prefix)` — shallow key enumeration of immediate children
- `sub(prefix)` — point-in-time `nlohmann::json` snapshot of an arbitrary subtree
- `set_default<T>(key, value)` — register in-memory fallback values that survive `reload()` and `clear()`
- `clear_defaults()` — remove all registered default values
- `bind_env(key, env_var)` — explicit per-key environment variable binding, complementing the prefix-based `StoreOptions::env_prefix`
- `merge(json)` / `merge_file(path, Path)` — deep-merge a JSON object overlay into the live store
- `load_layered(paths, Path)` — priority-ordered file stacking (later files win); silently skips absent files
- `start_watch(interval)` / `stop_watch()` — background file watcher thread that calls `reload()` automatically when the config file changes on disk
- `set_validator(fn)` / `clear_validator()` — schema validation callback; throw inside the function to reject an invalid config
- `dump(JsonFormat)` — serialize the entire store to a JSON string
- `get_root<T>()` / `set_root<T>(value)` — whole-root typed access for reading or overwriting the entire config at once
- `CONFIG_STRUCT` and `CONFIG_STRUCT_WITH_DEFAULT` macros — convenience wrappers around `NLOHMANN_DEFINE_TYPE_*` for struct serialization with optional-field support
- `[[nodiscard]]` annotations on all query methods (`get`, `get_root`, `get_or_set`, `contains`, `sub`, `all_keys`, `dump`, `save`)
- Google Benchmark integration in `benchmark/` replacing the previous hand-rolled timer harness
- Global free-function wrappers for all `ConfigStore` methods — delegate to `get_default_store()` so simple programs need zero setup
- `get_default_store()` — access the implicit store used by global free functions
- `release_store(path)` / `release_all_stores()` — explicit store-registry lifecycle management

### Changed

- Enum `GetStrategy` renamed to `MissingKeyPolicy` for clarity; values map directly: `ReturnDefault` → `DefaultValue`, `ThrowException` → `ThrowException`
- Enum `Obfuscate` renamed to `Encoding` to better reflect that the feature encodes values at rest
- `set()`, `remove()`, and `clear()` now return `void` and throw `SaveError` on auto-save failure instead of returning `bool`
- `connect()` now returns a `Connection` RAII handle instead of a `size_t` listener ID
- `config.hpp` split into a `detail/` subdirectory: enums and macros moved to `detail/types.hpp`, encoding helpers to `detail/obfuscation.hpp`, and path resolution to `detail/path_resolver.hpp`; `config.hpp` is now a thin public entry-point header

### Fixed

- `reload()` now calls the validator outside the mutex lock, preventing a deadlock if the validator itself calls any `ConfigStore` method
- `load_layered()` reads and merges all files before acquiring the write lock, so no intermediate (partially-merged) state is ever visible to concurrent readers
- `get_or_set()` uses a scoped lock block instead of a manual `unlock()` call, eliminating a potential lock-leak on exception
- `notify()` guards against an empty key when dispatching wildcard listeners, preventing undefined behavior on stores that use `on_any_change()`
- `get_store()` throws `std::logic_error` when a cached store is retrieved with a different `path_type` than it was originally created with, making the mismatch explicit rather than silently returning the wrong store
