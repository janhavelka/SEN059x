# 15 — Review coverage and hand off delivery

## Inputs

Apply [COMMON](COMMON.md). Consume `verified-package` and the [progress ledger](PROGRESS.md). Review `bench-evidence` or its recorded partial/blocked status if available. This stage produces `delivery-review`. A completed software review can carry outstanding hardware rows.

## Work

1. Trace each row in [COVERAGE](COVERAGE.md) from the manufacturer reference into public code, tests, example command/help and end-user documentation. Verify both SKUs, all 12 registers, all six published exchanges and all supported baud settings. Preserve the conditional and unresolved rows.
2. Compile or replay the examples the README actually gives. Compare [CONVENTIONS](CONVENTIONS.md) with exported headers, owner call order and product mapping. Check every human command against the real dispatcher and every runner record against [CLI_CONTRACT](CLI_CONTRACT.md). Resolve drift at the source and repeat affected checks.
3. Independently review stale-data behavior after failure, raw exceptions, public observer purity, identity changes, cancellation, late replies, echo ambiguity, unsuccessful write, recovery and persistence. Review the two FieldCore fixture modes at their pinned revisions, and identify any further work a real product integration requires.
4. Complete the evidence matrix by SKU, target, version, commit and fixture. Separate observed measurements, fake transports, firmware builds, archive installation and documentation examples. Publish concise user-facing setup/first measurement, commissioning and recovery directions. State which results remain provisional and why.
5. Update [PROGRESS](PROGRESS.md), user documentation and changelog with the delivered surfaces, exact checks and remaining work. Sync the agreed branch. Carry open issues as specific gaps with a reproduction or needed observation. Release version changes, publication and firmware integration require their own scope.

## Acceptance

Re-run the promptset, references, native, package, examples and applicable runner checks from [VERIFICATION](VERIFICATION.md). Verify the final commit against the remote and its CI status. Do a cold read from the public README as a library user, a bench operator and a shared-bus firmware maintainer. Each should find a working entry point and the evidence limits that affect their use.

## Handoff

Record `delivery-review` with the checked commit, package contents, exact example commands, consumer source revisions, CI evidence, observed SKU/fixture combinations and unfinished physical observations. Do not mark a physical row passed from a desktop test or compilation.
