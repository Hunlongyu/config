# Migrating to 0.2.2

[中文](MIGRATION.zh-CN.md) · [API](API.md)

## From 0.2.0 / 0.2.1

| Previous call | 0.2.2 |
|---|---|
| `get<T>(pointer)` | Strict read with owning result types and checked scalar numeric conversion. |
| `get_or<T>(pointer, fallback)` | `get<T>(pointer, fallback)`; no compatibility alias. |
| `get_or(pointer, fallback)` | `get<T>(pointer, fallback)`; specify the return type explicitly. |
| Caller-computed fallback | `get<T>(pointer, factory)` computes it only when missing. |
| `contains(pointer)` followed by `get<T>(pointer)` | `find<T>(pointer)` returns an optional from one snapshot. |
| `get<std::string_view>` / raw-pointer results | Use an owning type such as `std::string`; direct borrowed result types are rejected. |
| Implicit numeric truncation or wrapping | Fix the configuration or explicitly convert a JSON snapshot with an application-defined policy. |
| Integer/pointer fallback for bool | Supply bool, or a zero-argument factory returning bool. |
| `void erase(pointer)` | Returns bool for removal from the writable layer; false skips validation, writes, revision/dirty changes and events. |
| Parse key names from error text | Read `Error::key()` for key-specific failures. |
| `std::string version()` | Returns `constexpr std::string_view` and is noexcept; use `std::string(config::version())` when needed. |

Factories take no arguments and return a value implicitly convertible to `T`. Bool targets require bool; numeric targets require non-bool arithmetic values. Numeric fallback values/results undergo the same range and fractional-to-integer checks as scalar reads. Factories run synchronously, at most once, outside locks, and are not retained or retried. Exceptions thrown by the factory propagate unchanged. Existing null values, wrong types and invalid paths do not trigger a factory, and fallback results are not written back.

`find<T>` returns nullopt only for absence; null and invalid values still follow normal conversion rules. Read/query methods are nodiscard; use an explicit `(void)` cast for intentional discarded results. Container elements and custom deserializers must own their data and remain responsible for their member-conversion policy.

`update(fn)` now accepts callable objects directly, including move-only captures. Existing ordinary callbacks keep the same once-per-call transaction behavior. `snapshot` rejects unsupported View values instead of returning the writable layer. Create active Subscription handles through `Store::subscribe`; constructing one from an internal slot is no longer public.

The file format remains `config/v1`; 0.2 files do not need conversion. The fetched JSON dependency is now 3.12.0.

## From 0.1

This release deliberately removes duplicate and implicit APIs. There is no compatibility wrapper that retains hidden global state.

| 0.1 | 0.2.2 |
|---|---|
| ConfigStore with positional policies | Store(file, StoreOptions) |
| Global get/set/default store/get_store/release_store | An explicitly owned Store shared with callers |
| Path::Relative / Absolute | std::filesystem::path |
| Path::AppData | user_config_directory(application) / filename |
| SaveStrategy::Auto / Manual | SaveMode::automatic / manual in immutable options |
| MissingKeyPolicy, set_missing_key_policy | get<T>(pointer) is strict; get<T>(pointer, fallback) explicitly supplies a fallback |
| get(key, fallback) | get<T>(pointer, fallback) |
| Bare key or empty root key | /key; root uses snapshot/update |
| remove / clear / set_root | erase / update(fn) |
| get_root / sub / dump | snapshot / get<json> / snapshot().dump() |
| keys / children / all_keys / get_all | Inspect the JSON snapshot using items or JSON Pointer traversal |
| get_or_set | update(fn) with an explicit writable-layer initialization rule |
| set_default / clear_defaults | StoreOptions.defaults; no mutable source policy |
| bind_env / load_layered | StoreOptions.env_bindings / layers; reload refreshes sources |
| connect / on_change / on_any_change / numeric disconnect | subscribe(Change callback), Subscription::disconnect |
| start_watch / stop_watch | Separately scoped Watcher |
| get_store_path / path | path() |
| CONFIG_STRUCT macros | Standard nlohmann NLOHMANN_DEFINE_TYPE_* macros |
| JsonFormat::Pretty / Compact | Format::pretty / compact |
| Encoding::Base64 etc. | Encoding::base64, hex, rot13, reverse, combined, none |

Read [the API contracts](API.md) before migrating error handling. save now returns void and throws Error. Automatic save failure does not change memory or notify. Type errors never masquerade as missing values. Validators cover all candidates, and merge/reload participate in effective change notifications.

Defaults, file layers and environment are no longer saved back as if they were user edits. Explicit reload discards unsaved changes; Watcher refuses to discard them. Concurrent transactions may report conflict, and edit callbacks are never retried implicitly.

## Existing files

Plain JSON objects import automatically. Saving writes a config/v1 envelope with values and encoding fields.

For an encoded 0.1 file:
1. Make a backup of the original file.
2. Open it once with StoreOptions.legacy_encoding=true.
3. Read/validate the decoded values and call save().
4. Reopen with the normal options; subsequent files are versioned.

Only enable this flag for an actual old encoding file: it explicitly interprets __obfuscate_meta__ as metadata. The default importer preserves this name as ordinary business data. Reversible encodings do not protect secrets.

Optional layers now skip only absence; corrupt optional files are errors. Unknown encoding rules, invalid roots and unsupported format versions also fail explicitly. Update applications that previously relied on silent empty fallback.

## Removed test duplication

The former combined/persistence tests have one behavior-based suite. Former regression cases map to subscriptions, concurrency, persistence and sources. Registry-reference tests are replaced with single-owner and subscription-lifetime tests; mutable-policy race tests are replaced by immutable options and transaction tests. Legacy enum aliases, global functions, implicit root handling and program-name fallbacks are intentionally removed, rather than preserved as competing contracts.
