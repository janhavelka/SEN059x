# 02 — Establish the portable value and status API

## Inputs

Consume `baseline` from [01](01-baseline-and-coverage.md). Apply [COMMON](COMMON.md) and the literal source-to-API mapping in [CONVENTIONS](CONVENTIONS.md). Inspect the pinned SHZK public header and its FieldCore caller before drafting the interface. Produce `value-status`.

## Work

1. Add the normal package include `<SEN059x/SEN059x.h>` and the small self-contained headers it exports under `include/SEN059x/`. Place compiled portable code in `src/` and wire it into the existing component/package build. Preserve `Version.h`. Use only the C++17 standard facilities needed on native and embedded targets. Document public arguments, return values, buffer sizes and lifetimes with Doxygen.
2. Add `DeviceModel::SEN0591` and `DeviceModel::SEN0592`. Define the minimal value types for explicit binding and retained request context. Keep model and destination, FC, exact starting register and expected quantity or write value available across the exchange. Make invalid or incompletely populated context fail before use. A caller must select the model deliberately. Both valid models can have the same address and return identical bytes.
3. Implement the complete `Err`, `Status`, `Ok()`, string helpers and `ReadResponseError` contract in CONVENTIONS. Document the producer of every code. Preserve a negative platform detail and a positive raw exception byte through copies. Use static storage for messages. Define fallible value outputs to be assigned only on success and byte builders to return zero on failure. Document how the structured validator detail is always set, including when validation fails.
4. Keep data needed by the example bus owner expressible as plain values. It can represent a completed operation status, optional response-shape reason, model, address, raw word, time validity and units. Give transaction admission or progress its own owner state when that layer is implemented. No framework types, protocol strings used for branching, polling task or default device I/O in a public header.

## Acceptance

- Establish the desktop CMake/CTest harness under `test/native/` as specified in [VERIFICATION](VERIFICATION.md). Build and link a native C++17 consumer using the installed-style include. Compile each new public header first in a fresh translation unit. Exercise both models, explicit boolean conversion, both success predicates, every `errToString` case and fallback, known and unknown exception text, and signed details such as −7 and 0xE1.
- Add a regression for an invalid enum cast, zero or reserved unicast address, and a binding with no explicit model through the intended public validation route. Distinguish invalid binding from an unsupported profile and preserve the expected status/detail contract.
- Run the existing version/package checks and build the package consumer affected by the new exports. Record commands and exact headers in the ledger.

## Handoff

Publish `value-status` in [PROGRESS](PROGRESS.md). Give the next stage one real host consumer call, the structured error declarations and how to compile/link the package.
