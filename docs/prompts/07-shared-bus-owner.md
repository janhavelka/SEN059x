# 07 — Exercise a single RS485 owner on a shared bus

## Inputs

Apply [COMMON](COMMON.md). Use `value-status`, `rtu-codec`, `measurements`, `configuration` and `device-health` from stages 02–06. This stage produces `owner-contract`. Read the [ownership notes](../ARCHITECTURE.md), [RTU timing and frame layout](../reference/protocol/Modbus_RTU_notes.md), [per-model response times](../reference/measurement-and-configuration.md) and [consumer source map](SOURCE_MAP.md).

## Work

1. Implement one application-side owner path that can be driven by a scripted UART and clock on a desktop and reused by the later examples. Keep platform GPIO/UART calls behind that boundary. The owner configures the link, asserts DE, writes, waits for physical TX drain, returns to receive, collects one complete frame and calls the portable parser. Give the caller a single bounded step or a next-wake indication. Keep scheduling, retries, time sources and buffers outside the published sensor codec.
2. Bind each device to its explicitly configured SKU, unicast address, baud and per-device cache. Admit at most one outstanding physical exchange on a line, including while the sensor is ranging. Reject duplicate active addresses on a shared line. Copy the pending address, function, register, word count or write value, request/step identity, generation and deadline through completion. The caller owns TX storage until copied and RX storage through parsing. Keep only owned values after the observation returns.
3. Use monotonic time with documented units and a finite command deadline. Include queue wait, turnaround, request and response serialization, register-specific sensor time and RTU closure. For a processed distance read allow for the table 1 estimate of up to 750 ms plus overhead. For real-time distance the corresponding estimate is 140 ms. Apply the selected SKU/register figures to temperature and echo. The manufacturer's estimates provide scheduling budgets. They do not establish a guaranteed latest possible reply. Preserve the original deadline across retries. Exercise deployment at 115200 8N1 and a slower supported rate. Use the serial guide's low-speed character-time rules and high-speed recommended timers when evaluating frame and inter-byte gaps.
4. Size and bound storage for eight-byte requests, seven-byte one-word replies, eight-byte write acknowledgements and five-byte exceptions. Preserve accessible length, total received length, overflow, short TX, physical drain, source/echo policy and frame boundary. Pass only accessible bytes to the codec. A CRC-valid prefix inside a larger, truncated or concatenated frame fails. An FC06 response can exactly equal the request. Require independently established reply origin for an acknowledgement. Keep ambiguous bytes and possible write effects available to the caller.
5. Make retry and settlement decisions explicit. A retried distance/echo read triggers another acquisition. After a timeout, cancel, disable or bus reset, retire the old callback generation and drain or otherwise settle possible late traffic before the next exchange. FC03 omits the register and has no wire transaction ID. A local generation, fixed idle interval or input flush alone cannot identify a delayed one-word reply. Defer an indistinguishable follow-up if the owner cannot establish attribution. Any accepted TX prefix requires bounded settlement before DE release and bus reuse. A failed drain leaves the bus unavailable pending explicit recovery.
6. After a lost, partial or ambiguous write acknowledgement, report uncertainty. Permit an explicit bounded readback once the line is settled. Use known old/new endpoints in isolated address/baud commissioning. A matching readback establishes the observed setting. Preserve the unknown baud of the baud-change acknowledgement and unknown power-cycle persistence. Recovery, periodic sampling, startup and cache/status inspection issue zero configuration writes.

## Acceptance

Provide a working read path for the example boards. For a fresh, unambiguous FC03 exchange, document how the backend assembles one exact CRC-checked frame, handles trailing bytes and observes end of reception. Report the precision its UART or buffered API actually provides. A bounded host observation can support a read while leaving exact wire timing unknown. After an ambiguous timeout, document a concrete recovery path that can reestablish attribution before the same device and reply shape are reused.

Drive the real owner entry point with deterministic events and inspect TX bytes, DE transitions, completion records and each device's cache. Cover these cases with native tests.

| Input or fault | Observable result |
| --- | --- |
| Four uniquely addressed sensors and two simultaneous submissions | One request on the wire at a time. Each selected register, timestamp and reading returns to its own binding. Queued work can expire before TX. |
| Slow processed reply, five-byte exception, then eligible independent request | The owner waits through ranging, accepts a complete exception with its raw code and can serve the next device once settled. |
| Fragmented reply, trailing byte after a valid prefix, overrun or UART error | Only a complete, attributable, clean frame reaches typed publication. Bounds survive a short final read and overflow. |
| Exact deadline boundary, late scheduler wake and cross-unit rounding | Use documented closure evidence for on-time delivery. Foreground dequeue time cannot prove a wire timestamp. No retry extends the original window. |
| Distance times out, temperature then requested at the same address, old callback or late frame arrives | Record the stale completion or uncertainty. The old word never becomes a temperature. Reuse resumes only under the documented settlement policy. |
| Partial write, echo only, cancel during drain, drain failure, explicit recovery | Preserve possible effects, release DE after settlement when possible, observe no automatic replay and no false write confirmation. |
| Cached read, health/status inspection or two consecutive polls with no work | No TX, no delay and no change to the last-good timestamp or value. |

## Handoff

Record `owner-contract` in [PROGRESS](PROGRESS.md). Identify the application-side files, runnable native checks and policy for uncertain RX/TX, and provide the call sequence the consumer and both framework examples can use. Distinguish fake wire evidence from physical direction, timing and echo observations.
