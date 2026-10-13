# Migrating from 0.1 to 0.2

[中文](MIGRATION.zh-CN.md) · [API](API.md)

This release deliberately removes duplicate and implicit APIs. There is no compatibility wrapper that retains hidden global state.

| 0.1 | 0.2 |
|---|---|
| ConfigStore with positional policies | Store(file, StoreOptions) |
| Global get/set/default store/get_store/release_store | An explicitly owned Store shared with callers |
| Path::Relative / Absolute | std::filesystem::path |
| Path::AppData | user_config_directory(application) / filename |
| SaveStrategy::Auto / Manual | SaveMode::automatic / manual in immutable options |
| MissingKeyPolicy, set_missing_key_policy | get is strict; get_or explicitly supplies a fallback |
| get(key, fallback) | get_or<T>(pointer, fallback) |
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
