# SEN059x console operator reference

Select the SKU from its part marking with `model SEN0591` (3 m) or `model SEN0592` (5 m). The default endpoint is address 1 at 115200 baud, 8N1. Use `help config set threshold` or substitute any setting key for its units, model limits and bus effects.

```text
Console
  help [command]                                List commands or explain one command. (alias ?)
  version                                       Show library and application identity. (alias ver)
  color [on|off]                                Show or change human output color.
  verbose [on|off]                              Show or change human diagnostics.
Target
  model [SEN0591|SEN0592]                       Show or select the sensor model.
  profile [standard|dfrobot-isolated]           Show or select the address policy.
  useaddr <address>                             Select the local slave address.
  host [baud <rate>]                            Show or configure the host UART.
Cached
  status                                        Show binding, bus owner and current operation.
  drv                                           Show the latest bus request and its results.
  health                                        Show communication health for the binding.
  stats                                         Show shared bus operation and transport counters.
  settings                                      Show desired and observed configuration.
  latest [processed|realtime|temperature|echo]  Show cached measurements. (alias sample)
Measurements
  read <processed|realtime|temperature|echo>    Read one measurement.
  watch <kind> <count> <interval_ms>            Schedule a finite series of reads.
Configuration
  config read <key|register>                    Read one setting.
  readbaud                                      Read the baud code and rate.
  readreg <register>                            Read one typed register.
Commissioning
  maintenance [on isolated|off]                 Show, grant or release one write.
  config set <key> <value> [confirm]            Preview or verify an addressed write.
  setaddr <address> [confirm]                   Alias for config set address.
  setbaud <rate> [confirm]                      Alias for config set baud.
  broadcast set <key> <value> [confirm]         Preview or send an isolated vendor write.
Block diagnostic
  readregs <start> <count>                      Read a contiguous raw block.
Jobs
  job status                                    Show the active or most recent operation.
  result [op]                                   Show the retained terminal result.
  cancel [op]                                   Stop the active operation. (alias stop)
Diagnostics
  diagnose                                      Check the addressed device. (alias diag)
  recover                                       Service host-side line recovery.
  selftest                                      Check six published Modbus vectors offline.
```

The configuration keys are `address`, `baud`, `polarity`, `threshold`, `angle`, `uart-unit`, `noise` and `range`. A write preview performs zero transfers. Confirm a write only on a physically isolated device after `maintenance on isolated`. The grant covers one write. An addressed write verifies with FC03 at the intended endpoint. A vendor send requires `profile dfrobot-isolated`. Use a separate addressed read for evidence of its effect.

A machine client attaches with `@1 session <nonce>`, choosing a new nonce on each attachment, then increments its request ID. Match the boot ID, session, request and operation on each accepted operation's terminal record. A successful live result has `code="OK"` and `data.disposition="Ok"`. `job status` and `result <op>` inspect pending or retained work without a transfer. [Schema and fields](prompts/CLI_CONTRACT.md#machine-channel-for-the-host-runner)

The captured exchanges show a SEN0591 temperature watch, a retained sample after an injected drain error, and SEN0592 threshold commissioning through both write paths. Each scenario starts with a fresh scripted device and clock. [Dispatcher transcript excerpts](examples/cli-transcript.txt)

To replay the full human and machine transcripts after the native build, run the following from the repository root. On Windows, append `.exe` to both executable paths.

```sh
python test/native/test_machine_protocol.py .pio/sen059x-native/machine_parity .pio/sen059x-native/human_terminal --transcript
```
