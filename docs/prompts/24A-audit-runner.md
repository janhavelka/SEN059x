# 24A — Audit runner correlation and fault campaigns

## Start

Re-read original prompts 23–24 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md) and use [PROGRESS](PROGRESS.md) to locate the original work. Find the client and campaign commits, current client source, fixture generators and original dispatcher streams. Locate the actual offline replay command and its expected result in the runner's quick start.

## Inputs

Original prompts 23–24: [23](23-runner-client.md), [24](24-runner-faults.md).

Consume `runner-client` and `runner-evidence`. Produce `runner-audit`.

## Work

Trace a valid asynchronous result and a reboot-after-write case through the stream reader. Check boot/session/request/operation correlation, malformed or oversized records, timeouts, cancellation, bounded recovery and single terminal results. Contrast expiry before TX with lost verification after one FC06. Count actual emitted writes, and inspect the following cached trust and sample state. Review any complexity introduced to parse the same protocol twice.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Acceptance

Run `python -m unittest discover -s test -v` and the documented offline replay against the real dispatcher fixture. Check both SKUs' twenty addressed operations, vendor send-only cases and the raw-block diagnostic. Inject a successful stale record from another session. Require the client to reject it and recover on a subsequent independent query. Attribute host transcripts and the three distinct firmware builds separately. [Machine protocol](CLI_CONTRACT.md#machine-channel-for-the-host-runner)

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Give one reviewer the client/stream code and another the independent operation oracle and fault transcripts. Have a third reviewer check scope and simplify duplicate fixture paths. Reproduce failures against the actual bytes and repeat suites affected by a repair. Use the [audit procedure](AUDIT.md) to record the challenge and reruns.

## Handoff

Finish with a concise report of the audit findings, fixes made, and final verification results. Record `runner-audit` in [PROGRESS](PROGRESS.md) with fixture origins, expected and observed operation counts, correlation and replay outcomes, fixes, rerun commands and unexercised hardware rows.
