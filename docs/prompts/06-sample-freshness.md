# 06 — Keep raw and usable samples fresh per binding

## Start

Apply [COMMON](COMMON.md), find `measurement-values` in [PROGRESS](PROGRESS.md), and run its native decode fixtures. Own the portable cache/publication helpers and clock-driven tests in `test/native/`. Stop with passive observation and explicit publication behavior.

## Inputs

Consume `measurement-values` and produce `measurements`. Use the [public conventions](CONVENTIONS.md), [operation cases](OPERATIONS.csv) and the [consumer ownership notes](../ARCHITECTURE.md).

## Work

1. Keep independent state per explicit binding, generation and measurement kind. Publish a new usable engineering value only for the retained, current request with acceptable quality. An out-of-range but attributable reply may update the raw observation while leaving the last usable value available with its original time. Keep the two observation identities and timestamps distinct.
2. Give callers a zero-I/O snapshot/freshness query using their monotonic `nowMs` and maximum age. Define exact age boundary and wrap-safe interval. Distinguish never sampled, unknown time, fresh, stale and unusable. Keep a legitimate timestamp of zero representable.
3. Reject stale generation completions after rebind. Configuration changes that only select desired mode or SKU must never initiate a read. A change of binding identity invalidates or re-keys the cache. A block diagnostic or configuration read never publishes a typed sample.

## Acceptance

Run native tests with at least two devices and independent timestamps. Query at the exact freshness threshold and the next millisecond, across counter wrap and with time unknown. Observe a valid sample followed by an exception, timeout, corrupt reply and valid out-of-range word. Check raw and usable histories, ages, validity bits and zero transfers from every passive query. Verify one register acquisition cannot refresh another. Exercise all four kinds on each SKU using the real decoder.

## Delegation and self-audit

Re-read original prompt 06 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate a fixture with repeated identical reply bytes from distinct operations. Have an independent reviewer inject a completion after rebinding and inspect every cache timestamp. Fix any false publication and rerun the rebind and wraparound cases.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `measurements` in [PROGRESS](PROGRESS.md) with cache APIs, freshness boundary, binding lifetime, example outcomes for raw/usable samples and the exact native checks.

Finish with a concise report of the audit findings, fixes made, and final verification results.
