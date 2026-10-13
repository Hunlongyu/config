# Architecture and test contracts

[中文](ARCHITECTURE.zh-CN.md) · [API](API.md)

## Boundaries

| Module | Responsibility |
|---|---|
| store.hpp | Small public facade, typed reads, writable operations. |
| detail/value.hpp | Read-result constraints and checked scalar numeric conversions. |
| options.hpp / error.hpp | Immutable construction options and structured failures. |
| subscription.hpp / detail/observer.hpp | Independent subscription lifetime and ordered event queue. |
| detail/state.hpp | Immutable committed snapshots, source resolution, validation and transaction commit. |
| detail/key_path.hpp | One JSON Pointer contract, merge and encoding-rule maintenance. |
| detail/json_codec.hpp / encoding.hpp | Versioned file envelope and reversible encodings. |
| backend.hpp / detail/file_backend.hpp / atomic_file.hpp | One storage extension point and atomic filesystem replacement. |
| watcher.hpp | Optional interruptible polling and retry; no thread inside Store. |
| paths.hpp / detail/platform.hpp | Optional user-directory helper and isolated platform declarations. |

No src directory is required for this header-only library. There is no global default Store, mutable policy registry or permanently retained reference cache.

## State and transactions

Committed snapshots contain writable data, resolved file layers, sampled environment, the effective view, source bytes, revision and dirty state. Readers copy a shared snapshot handle under a short shared lock, then convert values outside locks.

All three get overloads share one path parse and snapshot read. Missing-value factories and update callbacks are accepted as templates, so move-only captures work without type erasure; each runs synchronously once and is not retained. Fallbacks do not write configuration or reread/retry after a concurrent commit. Stored subscription, validation and watcher callbacks retain their existing owner-managed lifetimes.

Find retains one snapshot through presence testing and conversion. Get/find reject direct borrowed result types, and scalar numeric conversions check bounds before casting; ordinary container/custom deserializers retain their own member-conversion contracts. Key-specific errors include the original pointer. An absent writable erase returns false before validation or commit, preserving revision, dirty state and storage.

Writers prepare and validate candidates without internal locks. At commit, a writer mutex serializes the revision check, optional persistence and publication. A changed revision produces a conflict instead of overwriting newer state. User edit functions are never automatically retried. Save acquires the same mutex before taking its snapshot.

Automatic mutations persist before publication. Manual changes publish and mark dirty. Explicit reload replaces unsaved writable state; background reload refuses to discard it. Source bytes are acknowledged only with a successful commit; rejected versions remain retryable.

Events are queued in commit order and delivered without state/writer locks. Subscriptions own callback slots; the Store tracks weak handles. Closing the Store deactivates outstanding handles, and callback captures are destroyed outside observer locks. Reentrant mutations queue subsequent events rather than recursively dispatching them.

## Files and ownership

Normalization resolves absolute paths and existing symbolic links. Active equivalent paths, including existing hard links and Windows case aliases, cannot have independent owners in the same linked program image. Ownership entries are weak and expire.

This is not interprocess locking or a coordination mechanism between separately linked DLL copies. Applications needing multiple processes must arrange external ownership; arbitrary external writes can race with an explicit save. The watcher is a reload facility, not a distributed transaction protocol.

The default backend serializes complete content before creating a unique sibling temporary file, writes and closes it, then replaces the target. Failure removes only its own temporary file and preserves the old target. This guarantees complete-file replacement under normal filesystem operation; it does not claim power-loss durability or preserve all platform-specific ACL/metadata.

## Tests

Each case owns a unique temporary directory. Tests never delete application-data directories. Suites are grouped by behavior:

| Suite | Acceptance |
|---|---|
| store | Types, structs, three get overloads, callback counts/exceptions, move-only captures, strict paths, view validation, root updates, alias ownership. |
| reads | Compile-time result/fallback constraints, scalar numeric boundaries, optional lookup, owning results, structured key errors and snapshot lifetime under concurrent writes. |
| persistence | Automatic rollback, manual dirty state, absent-erase no-ops, failed-erase rollback, corruption, serialization/write faults, Unicode paths, encoding migration. |
| sources | Precedence, consistent views, environment isolation, shared codec, all-entry validation. |
| subscriptions | All mutation events, ordering, reentrancy, disconnect, exception handling and destruction. |
| concurrency | Gated save overlap, stale reload/erase rejection, writes during fallback computation, one-shot updates, exact parallel increment count. |
| watcher | Same-timestamp edits, retry, conflict, dirty-state protection, cancellation and lifetime. |
| consumers | Independent public headers, multiple translation units, parent-provided dependencies, installed package and manual integration. |
| examples / docs | Build and execute examples; compare README snippets against their compiled source and verify local links. |

The save regression blocks the first backend write before starting a second mutation/save. A generated test-only header copy removes the save mutex; the same regression must fail. CTest treats that rejection as the mutation guard passing. This is evidence that the test checks the protection, not a percentage coverage claim.

CI builds and tests Windows, Linux and macOS, plus Linux ASan/UBSan. Local Windows ASan is available through CONFIG_SANITIZER. CI coverage reporting remains a diagnostic metric; it does not replace behavioral assertions or the mutation guard. Local execution does not prove other operating systems or filesystem crash behavior.
