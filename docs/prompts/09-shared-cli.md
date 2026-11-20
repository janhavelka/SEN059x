# 09 — Implement the shared bench console

## Inputs

Apply [COMMON](COMMON.md), the [CLI contract](CLI_CONTRACT.md) and the [public conventions](CONVENTIONS.md). Consume `device-health`, `owner-contract` and `consumer-fixture`. This stage produces `shared-cli`. Inspect the pinned command tables and dispatchers in the CLI contract and [source map](SOURCE_MAP.md).

## Work

1. Put the framework-independent example processor, bounded line editor, registry, formatting and dispatch in `examples/common/`. The only dependencies for a native build are the standard C++ library, SEN059x and the application-side owner types. Use injected hooks for console output, clock, boot/build identity, board capabilities and commissioning admission. Keep example code outside library exports.
2. Implement every command and alias in the contract. From one registry, provide discoverable grouped help, per-command usage and a checked command list for both example READMEs. Specify arity, units, supported models and local/cached/live/commissioning effects. A preview, help, health or latest query must work with a fake port that fails any attempted I/O.
3. Route live work through the normal owner entry point. Keep each pending operation's request ID, generation, model, target, register/expected value, deadline and terminal storage alive. Allow passive queries and cancellation during a long processed read. Keep cancellation, write readback, late-reply settlement, retained data and primary/cleanup results observable. Give previews and failed admissions zero bus effects. Keep sessions and local target changes isolated.
4. Implement both renderers and the versioned runner handshake. In human mode show text that an operator can use with a serial terminal. In machine mode implement exact framing, correlation, terminal reservation and signed status detail from the [CLI contract](CLI_CONTRACT.md). Add a short operator reference, a human transcript and machine examples from the real dispatcher running against a scripted owner.

## Acceptance

- Compile the shared C++ sources and dispatch them in the native test target using a fake UART, clock, capped output sink and fixed boot ID. Feed complete command streams through the line editor and dispatcher.
- Prove help/registry/docs agree, aliases resolve identically and arity, maximum length, CR/LF, editing, bad integers, overflow poison, unknown registers and unsupported rates have defined outcomes. After a malformed line send a valid one and verify it works.
- Exercise all four live measurements and eight configuration addresses across both model profiles. Use a positive temperature sample, ambiguous temperature raw value, echo, exact exception, stale cache, SKU-specific range rejection, wrong FC06 echo, confirmed readback and uncertain write. Assert units, literal public statuses, sample/health counters and per-command transfer counts.
- Interleave a finite watch or delayed read with `health`, `job status`, a second live request and `stop`. Submit an old generation, rebind, repeat a request ID, reattach during pending work and stall the sink across completion. Assert one attributable terminal per admitted request and drain/cleanup evidence. The half-open operation must survive operator and runner backpressure.
- Capture a transcript starting at the prompt, including help, selection, successful read, failed read, retained last-good age, `drv` and a commissioning preview. Capture a complete JSON session and terminal failure with signed detail. Leave board/build integration to the next stages.

## Handoff

Record `shared-cli` in [PROGRESS](PROGRESS.md) with the shared file paths, native test command, schema limit, transcripts and any remaining platform-only obligations. Stages 10 and 11 link the same processor and stage 12 drives this grammar externally.
