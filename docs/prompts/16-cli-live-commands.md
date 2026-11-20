# 16 — Run live reads, diagnostics and jobs from the terminal

## Start

Apply [COMMON](COMMON.md). Replay the native help stream from [PROGRESS](PROGRESS.md), then locate the actual owner submission and terminal-result paths. Own shared example handlers, operator help and native transcripts for reading and jobs. Stop after human-mode live operation coverage.

## Inputs

Consume `cli-console`. Produce `cli-live`. Use the [human command table](CLI_CONTRACT.md#human-console), [configuration key table](CLI_CONTRACT.md#configuration-commands-for-both-example-applications) and [operation cases](OPERATIONS.csv).

## Work

1. Implement the four measurement commands, eight `config read` keys and numeric forms, `readbaud`, and all twelve one-word `readreg` paths through the typed public API. Implement `diagnose`/`diag` and owner-side `recover`. Show register, units, raw word, model, sample generation, age, elapsed time and retained previous data on failure.
2. Implement `readregs <start> <count>` with one addressed FC03, bounded timeout, exact count and ordered raw words. Validate contiguity and bounds before TX. Keep typed sample/configuration caches unchanged even for count one. Report an exception or timeout without retries or split fallback.
3. Implement finite `watch`, `job status`, `result [op]` and `cancel [op]`/`stop`. Keep request context through completion, schedule one due read at a time and report skipped intervals. Service queries and cancellation during ranging, retain terminal results and reject stale generations.

## Acceptance

Rebuild the native test target and run `ctest --test-dir .pio/sen059x-native -C Release --output-on-failure`. [Native build recipe](VERIFICATION.md)

At the human prompt verify all twelve reads for both models with the expected frames and data. Include a positive and ambiguous temperature, echo, late response and exception. Test three-word success, bad count, gap, wrap, broadcast, exception and timeout on block reads. During watch inject a missing reply, a busy submission, stale cancellation and a cached query. Assert transfer counts, last-good timestamp, finite termination and one terminal result per accepted job.

## Delegation and self-audit

Use the [stage audit](AUDIT.md). Delegate read cases and have an independent reviewer run an old generation during a delayed temperature request and a one-word raw diagnostic. Fix and replay any case that changes the wrong cache.

## Handoff

Record `cli-live` in [PROGRESS](PROGRESS.md) with runnable native tests, a human read/failure transcript, read coverage per SKU and job cancellation evidence.
