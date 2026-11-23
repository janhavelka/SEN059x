#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "Exchange.h"
#include "ExchangeTiming.h"

#include <SEN059x/SEN059x.h>

namespace {
using namespace SEN059x;
using namespace SEN059x::example;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

using Bytes = std::vector<std::uint8_t>;
// DFRobot examples 2 and 4. Both SKU manuals give these exact pairs.
const Bytes REALTIME_REQUEST{1, 3, 1, 1, 0, 1, 0xD4, 0x36};
const Bytes REALTIME_REPLY{1, 3, 2, 2, 0xEF, 0xF8, 0xA8};
const Bytes ADDRESS_WRITE{1, 6, 2, 0, 0, 5, 0x48, 0x71};
// Synthetic three-register response and FC03 exception, with independently calculated CRCs.
const Bytes BLOCK_REQUEST{1, 3, 1, 0, 0, 3, 0x04, 0x37};
const Bytes BLOCK_REPLY{1, 3, 6, 2, 0xF2, 2, 0xEF, 1, 0x2C, 0x29, 0x42};
const Bytes EXCEPTION{1, 0x83, 0xE1, 0x81, 0x78};
const Bytes WRITE_EXCEPTION{1, 0x86, 0xE2, 0xC2, 0x29};

// Times in us from TX start, 8N1, 100 us turnaround and 140 ms real-time ranging.
// RX times describe when the first three and the last four bytes are all available.
struct Timeline {
  std::uint32_t bps;
  std::uint64_t drain;
  std::uint64_t firstBatch;
  std::uint64_t lastBatch;
  std::uint64_t close;
  std::uint64_t deadline;
};
constexpr Timeline FAST{115200, 695, 141056, 141403, 143153, 144153};
constexpr Timeline SLOW{9600, 8334, 151559, 155726, 159372, 160372};

struct Delivery {
  Bytes bytes;
  AckOrigin origin = AckOrigin::Device;
  std::optional<std::uint32_t> gapUs = std::nullopt;
  Status uart{};
  bool wireClosed = false;
  bool overrun = false;
};

// The script controls hardware evidence independently of the foreground clock. Empty polls
// have no source evidence. Even when the user buffer fills, the UART still counts extra bytes.
struct Port : ExchangePort {
  std::uint64_t time = 0;
  std::uint64_t directionSetupUs = 0;
  std::uint64_t directionReleaseUs = 0;
  std::uint64_t readDurationUs = 0;
  bool transmitting = false;
  bool drained = false;
  Status drainStatus;
  std::size_t writeLimit = 8;
  Bytes offered;
  unsigned writes = 0;
  unsigned reads = 0;
  std::vector<std::pair<std::uint64_t, bool>> directions;
  std::optional<Delivery> pending;

  std::uint64_t nowUs() const override { return time; }
  void setTransmit(bool enable) override {
    transmitting = enable;
    directions.emplace_back(time, enable);
    time += enable ? directionSetupUs : directionReleaseUs;
  }
  std::size_t write(const std::uint8_t* bytes, std::size_t size) override {
    CHECK(transmitting);
    ++writes;
    offered.assign(bytes, bytes + size);
    return std::min(size, writeLimit);
  }
  Status txDrained(bool& out) override {
    CHECK(transmitting);
    out = drained;
    return drainStatus;
  }
  RxBatch read(std::uint8_t* dst, std::size_t capacity) override {
    CHECK(!transmitting);
    ++reads;
    RxBatch out{};
    if (pending) {
      out.total = pending->bytes.size();
      out.copied = std::min(out.total, capacity);
      if (out.copied) {
        std::copy_n(pending->bytes.data(), out.copied, dst);
      }
      out.origin = pending->origin;
      out.wireGapUs = pending->gapUs;
      out.uartStatus = pending->uart;
      out.wireFrameClosed = pending->wireClosed;
      out.overflow = pending->overrun;
      pending.reset();
    }
    time += readDurationUs;
    return out;
  }
};

constexpr auto REALTIME = MeasurementKind::RealTimeDistance;
struct Fixture {
  Port port;
  std::array<std::uint8_t, 16> rx{};
  MeasurementCache cache;
  DeviceHealth health;
  Exchange exchange;
  ExchangeRequest request{};

  explicit Fixture(const Timeline& t = FAST, std::size_t capacity = 16,
                   DeviceModel model = DeviceModel::SEN0591)
      : exchange(port, rx.data(), capacity) {
    CHECK(capacity <= rx.size());
    rx.fill(0xCD);
    CHECK(cache.bind(model, 1));
    CHECK(health.bind(model, 1));
    request.operationId = 37;
    request.bindingGeneration = cache.snapshot(REALTIME, 0, 0).bindingGeneration;
    request.context = {model, 1, FunctionCode::ReadHoldingRegisters, 0x0101, 1};
    request.baud = t.bps;
    budget();
    CHECK(request.deadlineUs == t.deadline);
    request.publishMeasurement = true;
  }

  void budget(std::optional<std::uint32_t> deviceUs = std::nullopt) {
    ExchangeTiming timing{};
    CHECK(exchangeTiming(request.context, request.baud, 100, 1000, deviceUs, timing));
    request.deadlineUs = port.time + timing.budgetUs;
  }
  std::uint64_t poll(std::uint64_t now) {
    CHECK(now >= port.time);
    port.time = now;
    return exchange.service();
  }
  std::uint64_t deliver(std::uint64_t now, Delivery delivery) {
    CHECK(!port.pending && !exchange.done());
    port.pending = std::move(delivery);
    const auto wake = poll(now);
    CHECK(!port.pending);
    return wake;
  }
  void send(const Bytes& expected) {
    const auto start = port.time;
    const auto writes = port.writes;
    CHECK(exchange.begin(request, health, request.publishMeasurement ? &cache : nullptr));
    CHECK(port.writes == writes && !port.transmitting);
    poll(start);
    CHECK(port.transmitting && port.writes == writes + 1 && port.offered == expected);
    CHECK(port.directions.back().first == start && port.directions.back().second);
  }
  void drain(std::uint64_t at) {
    const auto reads = port.reads;
    CHECK(!port.drained);
    const auto nextWake = poll(at - 1);
    CHECK(!exchange.done() && port.transmitting && port.reads == reads);
    CHECK(nextWake > at - 1 && nextWake <= request.deadlineUs);
    port.drained = true;
    poll(at);
    CHECK(!port.transmitting && port.directions.back().first == at);
    CHECK(!port.directions.back().second && exchange.result().txDrained);
  }
  SampleSnapshot sample() const {
    return cache.snapshot(REALTIME, static_cast<std::uint32_t>(port.time / 1000), 1000);
  }
  // A terminal service call must retain evidence and must not generate another bus or health event.
  void terminal() {
    CHECK(exchange.done());
    const auto saved = exchange.result();
    const auto before = health.healthSnapshot();
    const auto history = sample();
    const auto reads = port.reads;
    const auto writes = port.writes;
    const auto edges = port.directions.size();
    poll(port.time + 1);
    CHECK(exchange.done() && port.reads == reads && port.writes == writes);
    CHECK(port.directions.size() == edges);
    const auto& after = exchange.result();
    CHECK(after.status.code == saved.status.code && after.status.detail == saved.status.detail);
    CHECK(after.rxTotal == saved.rxTotal && after.request.operationId == saved.request.operationId);
    CHECK(health.healthSnapshot().totalSuccess == before.totalSuccess);
    CHECK(health.healthSnapshot().totalFailures == before.totalFailures);
    CHECK(sample().usable.sampleGeneration == history.usable.sampleGeneration);
    CHECK(sample().raw.sampleGeneration == history.raw.sampleGeneration);
  }
};

void fragmented(Fixture& f, const Timeline& t) {
  const auto expected = f.request;
  f.send(REALTIME_REQUEST);
  f.drain(t.drain);
  // Recycle caller-owned operation data. The owner must keep the original identity.
  f.request = {};
  f.deliver(t.firstBatch, {{1, 3, 2}});
  CHECK(!f.exchange.done() && f.exchange.result().rxAccessible == 3);
  f.deliver(t.lastBatch, {{2, 0xEF, 0xF8, 0xA8}});
  CHECK(!f.exchange.done() && f.health.healthSnapshot().totalSuccess == 0);
  CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
  CHECK(f.poll(t.close - 1) == t.close && !f.exchange.done());
  f.poll(t.close);
  const auto& r = f.exchange.result();
  CHECK(f.exchange.done() && r.status && r.txAccepted == 8 && r.txDrained);
  CHECK(r.rxAccessible == 7 && r.rxTotal == 7 && !r.overflow && r.origin == AckOrigin::Device);
  CHECK(!r.wireFrameClosed && !r.wireGapError && r.uartStatus);
  CHECK(r.lastDeliveryUs == t.lastBatch && r.completedUs == t.close);
  CHECK(r.request.operationId == expected.operationId);
  CHECK(r.request.bindingGeneration == expected.bindingGeneration && r.request.baud == t.bps);
  CHECK(r.request.context.address == 1 && r.request.context.model == expected.context.model);
  CHECK(r.request.context.startRegister == 0x0101 && r.request.context.countOrWriteWord == 1);
  CHECK(r.request.deadlineUs == t.deadline);
  const auto sample = f.sample();
  CHECK(sample.raw.measurement.rawWord == 0x02EF && sample.usable.measurement.value == 751);
  CHECK(sample.usable.measurement.unit == MeasurementUnit::Millimetres);
  CHECK(sample.usable.measurement.completedMs == t.close / 1000);
  const auto health = f.health.healthSnapshot();
  CHECK(health.totalSuccess == 1 && health.totalFailures == 0 && health.lastOkTimeValid);
  CHECK(health.lastOkMs == t.close / 1000);
  f.terminal();
}

void checkExtraBytes(std::size_t capacity, const Bytes& suffix) {
  Fixture f(FAST, capacity);
  f.request.deadlineUs += 10000;
  f.send(REALTIME_REQUEST);
  f.drain(FAST.drain);
  f.deliver(FAST.lastBatch, {REALTIME_REPLY});
  CHECK(!f.exchange.done() && f.sample().raw.freshness == Freshness::NeverSampled);
  // The old silence timer fires while the UART has more bytes. Inspect it before closing.
  f.deliver(FAST.close, {suffix});
  f.poll(FAST.close + 1750);
  const auto& r = f.exchange.result();
  CHECK(f.exchange.done() && !r.status && f.health.healthSnapshot().totalSuccess == 0);
  CHECK(r.rxTotal == 7 + suffix.size() && r.rxAccessible == std::min(capacity, r.rxTotal));
  CHECK(r.overflow == (r.rxTotal > capacity));
  CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
  if (capacity < f.rx.size()) {
    CHECK(f.rx[capacity] == 0xCD);
  }
  f.terminal();
}

void checkProcessed() {
  Fixture f(SLOW);
  f.request.context.startRegister = 0x0100;
  f.budget();
  CHECK(f.request.deadlineUs == 770372);
  f.send({1, 3, 1, 0, 0, 1, 0x85, 0xF6});
  f.drain(8334);
  f.poll(750000);
  CHECK(!f.exchange.done());
  f.deliver(765726, {{1, 3, 2, 2, 0xF2, 0x38, 0xA1}});
  f.poll(769372);
  CHECK(f.exchange.result().status && f.health.healthSnapshot().totalSuccess == 1);
  const auto processed = f.cache.snapshot(MeasurementKind::ProcessedDistance, 769, 0);
  CHECK(processed.usable.measurement.value == 754 &&
        processed.usable.measurement.completedMs == 769);
  f.terminal();
}

void checkPollAcrossClosure() {
  Fixture f;
  f.request.deadlineUs += 10000;
  f.send(REALTIME_REQUEST);
  f.drain(FAST.drain);
  f.deliver(FAST.lastBatch, {REALTIME_REPLY});
  // The UART checks an empty queue before t3.5. Returning across t3.5 requires a fresh check.
  f.port.readDurationUs = 2;
  const auto wake = f.poll(FAST.close - 1);
  CHECK(!f.exchange.done() && wake == f.port.time);
  f.deliver(wake, {{0}});
  f.poll(f.port.time + 1750);
  CHECK(f.exchange.done() && !f.exchange.result().status && f.exchange.result().rxTotal == 8);
  CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
  CHECK(f.health.healthSnapshot().totalSuccess == 0);
  f.terminal();
}

void checkQueuedBatches() {
  Fixture f;
  f.send(REALTIME_REQUEST);
  f.drain(FAST.drain);
  // One UART event can queue more than one bounded read before the foreground is scheduled.
  const auto at = f.request.deadlineUs - 1;
  const auto wake = f.deliver(at, {{1, 3, 2}});
  CHECK(wake == at && !f.exchange.done());
  f.deliver(wake, {{2, 0xEF, 0xF8, 0xA8}, AckOrigin::Device, std::nullopt, Ok(), true});
  CHECK(f.exchange.done() && f.exchange.result().status && f.exchange.result().completedUs == at);
  CHECK(f.health.healthSnapshot().totalSuccess == 1 && f.sample().usable.measurement.value == 751);
  f.terminal();
}

void checkTxFailures() {
  Fixture inactive;
  CHECK(inactive.exchange.begin(inactive.request, inactive.health, &inactive.cache));
  CHECK(inactive.exchange.begin(inactive.request, inactive.health, &inactive.cache).code ==
        Err::BUS_BUSY);
  inactive.poll(FAST.deadline);
  CHECK(inactive.exchange.result().status.code == Err::TIMEOUT);
  CHECK(inactive.port.directions.empty() && inactive.port.writes == 0);
  CHECK(inactive.health.healthSnapshot().totalFailures == 0);
  inactive.terminal();

  Fixture setup;
  setup.port.directionSetupUs = 10;
  CHECK(setup.exchange.begin(setup.request, setup.health, &setup.cache));
  setup.poll(FAST.deadline - 10);
  CHECK(setup.exchange.result().status.code == Err::TIMEOUT &&
        setup.exchange.result().txAccepted == 0);
  CHECK(!setup.port.transmitting && setup.port.writes == 0);
  CHECK(setup.health.healthSnapshot().totalFailures == 0);
  setup.terminal();

  Fixture zero;
  zero.port.writeLimit = 0;
  CHECK(zero.exchange.begin(zero.request, zero.health, &zero.cache));
  zero.poll(0);
  CHECK(zero.port.offered == REALTIME_REQUEST && zero.port.writes == 1);
  CHECK(!zero.port.transmitting && zero.exchange.result().txAccepted == 0);
  CHECK(!zero.exchange.result().txDrained && zero.exchange.result().status.code == Err::IO);
  CHECK(zero.health.healthSnapshot().totalFailures == 0);
  zero.terminal();

  Fixture partial;
  partial.port.writeLimit = 3;
  partial.send(REALTIME_REQUEST);
  partial.drain(FAST.drain);
  CHECK(partial.exchange.done() && partial.exchange.result().status.code == Err::IO);
  CHECK(partial.exchange.result().txAccepted == 3 && partial.port.reads == 0);
  CHECK(partial.health.healthSnapshot().totalSuccess == 0);
  partial.terminal();

  Fixture stuck;
  stuck.send(REALTIME_REQUEST);
  stuck.poll(FAST.deadline - 1);
  CHECK(stuck.port.transmitting && !stuck.exchange.done());
  stuck.poll(FAST.deadline);
  CHECK(stuck.exchange.done() && stuck.port.transmitting && !stuck.exchange.result().txDrained);
  CHECK(stuck.exchange.result().txAccepted == 8 && !stuck.exchange.result().status);
  CHECK(stuck.port.reads == 0 && stuck.health.healthSnapshot().totalSuccess == 0);
  CHECK(stuck.health.healthSnapshot().totalFailures == 0);
  CHECK(stuck.exchange.begin(stuck.request, stuck.health, &stuck.cache).code == Err::BUS_BUSY);
  stuck.terminal();

  Fixture failed;
  failed.send(REALTIME_REQUEST);
  failed.port.drainStatus = {Err::IO, -33, "UART drain error"};
  failed.poll(FAST.drain);
  CHECK(failed.exchange.done() && failed.exchange.result().status.detail == -33);
  CHECK(!failed.exchange.result().txDrained && failed.port.transmitting && failed.port.reads == 0);
  CHECK(failed.health.healthSnapshot().totalFailures == 0);
  failed.terminal();

  Fixture late;
  late.send(REALTIME_REQUEST);
  late.port.drained = true;
  late.port.directionReleaseUs = 2;
  late.poll(FAST.deadline - 1);
  const auto& r = late.exchange.result();
  CHECK(late.exchange.done() && r.status.code == Err::TIMEOUT && r.txDrained);
  CHECK(r.completedUs == FAST.deadline + 1 && !late.port.transmitting && late.port.reads == 0);
  CHECK(late.health.healthSnapshot().totalFailures == 0);
  late.terminal();
}

void checkDeadline(std::uint64_t finalPoll) {
  Fixture f;
  f.send(REALTIME_REQUEST);
  f.drain(FAST.drain);
  f.deliver(FAST.lastBatch, {REALTIME_REPLY});
  CHECK(!f.exchange.done());
  // Foreground polling missed closure. Pending bytes cannot retroactively meet the deadline.
  f.poll(finalPoll);
  CHECK(f.exchange.done() && f.exchange.result().status.code == Err::TIMEOUT);
  CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
  CHECK(f.health.healthSnapshot().totalSuccess == 0 &&
        f.health.healthSnapshot().totalFailures == 1);
  f.terminal();
}

void checkWireGap(const Timeline& t, std::uint32_t limitUs) {
  for (const auto gapUs : {limitUs, limitUs + 1}) {
    Fixture f(t);
    f.send(REALTIME_REQUEST);
    f.drain(t.drain);
    f.deliver(20000, {{1, 3, 2}});
    f.deliver(30000, {{2, 0xEF, 0xF8, 0xA8}, AckOrigin::Device, gapUs});
    f.poll(34000);
    CHECK(f.exchange.done() && f.exchange.result().status.ok() == (gapUs == limitUs));
    CHECK(f.exchange.result().wireGapError == (gapUs > limitUs));
    CHECK(f.sample().usable.measurement.value.has_value() == (gapUs == limitUs));
    f.terminal();
  }
}

void checkReadErrors() {
  for (bool badGap : {true, false}) {
    Fixture f;
    f.send(REALTIME_REQUEST);
    f.drain(FAST.drain);
    Delivery bytes{REALTIME_REPLY};
    if (badGap) {
      bytes.gapUs = 751;
    } else {
      bytes.uart = {Err::IO, -12, "UART overrun"};
      bytes.overrun = true;
    }
    f.deliver(FAST.lastBatch, bytes);
    f.poll(FAST.close);
    const auto& r = f.exchange.result();
    CHECK(f.exchange.done() && !r.status && r.wireGapError == badGap);
    CHECK(r.uartStatus.code == bytes.uart.code && r.uartStatus.detail == bytes.uart.detail);
    CHECK(r.overflow == bytes.overrun);
    CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
    CHECK(f.health.healthSnapshot().totalSuccess == 0);
    f.terminal();
  }

  Fixture ex;
  ex.send(REALTIME_REQUEST);
  ex.drain(FAST.drain);
  ex.deliver(FAST.lastBatch, {EXCEPTION});
  ex.poll(FAST.close);
  CHECK(ex.exchange.result().status.code == Err::EXCEPTION &&
        ex.exchange.result().status.detail == 0xE1);
  CHECK(ex.exchange.result().rxAccessible == 5 &&
        ex.sample().raw.freshness == Freshness::NeverSampled);
  CHECK(ex.health.healthSnapshot().totalSuccess == 1 &&
        ex.health.healthSnapshot().totalFailures == 0);
  ex.terminal();

  Fixture crc;
  crc.send(REALTIME_REQUEST);
  crc.drain(FAST.drain);
  Bytes damaged = REALTIME_REPLY;
  damaged.back() ^= 1;
  crc.deliver(FAST.lastBatch, {damaged});
  crc.poll(FAST.close);
  CHECK(crc.exchange.result().status.code == Err::CRC_ERROR);
  CHECK(crc.health.healthSnapshot().totalFailures == 1 &&
        crc.health.healthSnapshot().totalSuccess == 0);
  CHECK(crc.sample().raw.freshness == Freshness::NeverSampled);
  crc.terminal();
}

void checkForeignThenReply() {
  Fixture f;
  f.send(REALTIME_REQUEST);
  f.drain(FAST.drain);
  f.deliver(2700, {{2, 3, 2, 2, 0xEF, 0xBC, 0xA8}});
  f.poll(4450);
  CHECK(!f.exchange.done() && f.sample().raw.freshness == Freshness::NeverSampled);
  CHECK(f.health.healthSnapshot().totalFailures == 0 &&
        f.health.healthSnapshot().totalSuccess == 0);
  f.deliver(FAST.lastBatch, {REALTIME_REPLY});
  f.poll(FAST.close);
  CHECK(f.exchange.result().status && f.sample().usable.measurement.value == 751);
  CHECK(f.health.healthSnapshot().totalSuccess == 1 &&
        f.health.healthSnapshot().totalFailures == 0);
  f.terminal();
}

void checkSplitFrames() {
  Fixture f;
  f.send(REALTIME_REQUEST);
  f.drain(FAST.drain);
  // Two wire frames buffered until one foreground wake would make a valid reply if joined.
  f.deliver(10000, {{1, 3, 2}, AckOrigin::Device, std::nullopt, Ok(), true});
  f.port.pending = Delivery{{2, 0xEF, 0xF8, 0xA8}};
  CHECK(f.exchange.done() && f.exchange.result().status.code == Err::FRAME_ERROR);
  CHECK(f.exchange.result().readError == ReadResponseError::FRAME_TOO_SHORT);
  CHECK(f.exchange.result().rxTotal == 3 && f.exchange.result().wireFrameClosed);
  CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
  CHECK(f.health.healthSnapshot().totalSuccess == 0);
  f.terminal();
  CHECK(f.port.pending.has_value());
}

void checkErroredEcho() {
  for (bool errorOnly : {false, true}) {
    Fixture f;
    f.send(REALTIME_REQUEST);
    f.drain(FAST.drain);
    if (errorOnly) {
      f.deliver(1800, {{1, 3, 2}});
    }
    f.deliver(2000, {errorOnly ? Bytes{} : REALTIME_REQUEST,
                     AckOrigin::LocalEcho,
                     std::nullopt,
                     {Err::IO, -17, "framing error"}});
    CHECK(f.exchange.result().uartStatus.detail == -17 && !f.exchange.result().status);
    CHECK(f.health.healthSnapshot().totalFailures == 0 &&
          f.health.healthSnapshot().totalSuccess == 0);
    CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
    f.terminal();
  }
}

void checkWrite(AckOrigin origin, bool qualifiedClosure, const Bytes& bytes) {
  Fixture f;
  f.request.publishMeasurement = false;
  f.request.context.function = FunctionCode::WriteSingleRegister;
  f.request.context.startRegister = 0x0200;
  f.request.context.countOrWriteWord = 5;
  f.budget(10000);
  f.send(ADDRESS_WRITE);
  f.drain(FAST.drain);
  // TX has drained and DE is in RX. A buffered transceiver echo may still be delivered here.
  f.deliver(2000, {bytes, origin});
  if (qualifiedClosure && !f.exchange.done()) {
    f.deliver(3750, {{}, AckOrigin::Unknown, std::nullopt, Ok(), true});
  } else {
    f.poll(3750);
  }
  if (!f.exchange.done()) {
    f.poll(f.request.deadlineUs);
  }
  const bool valid = origin == AckOrigin::Device && qualifiedClosure && bytes == ADDRESS_WRITE;
  const bool deviceException =
      origin == AckOrigin::Device && qualifiedClosure && bytes == WRITE_EXCEPTION;
  const auto& r = f.exchange.result();
  CHECK(f.exchange.done() && r.status.ok() == valid && r.rxTotal == bytes.size());
  CHECK(r.origin == origin);
  if (deviceException) {
    CHECK(r.status.code == Err::EXCEPTION && r.status.detail == 0xE2);
  }
  CHECK(f.health.healthSnapshot().totalSuccess == ((valid || deviceException) ? 1u : 0u));
  CHECK(f.sample().raw.freshness == Freshness::NeverSampled);
  f.terminal();
}

void checkBufferedEchoThenAck() {
  Fixture f(FAST, 8);
  f.request.publishMeasurement = false;
  f.request.context = {DeviceModel::SEN0591, 1, FunctionCode::WriteSingleRegister, 0x0200, 5};
  f.budget(10000);
  f.send(ADDRESS_WRITE);
  f.drain(FAST.drain);
  const auto at = f.request.deadlineUs - 1;
  CHECK(f.deliver(at, {ADDRESS_WRITE, AckOrigin::LocalEcho, std::nullopt, Ok(), true}) == at);
  CHECK(!f.exchange.done() && f.health.healthSnapshot().totalSuccess == 0);
  f.deliver(at, {ADDRESS_WRITE, AckOrigin::Device, std::nullopt, Ok(), true});
  const auto& r = f.exchange.result();
  CHECK(f.exchange.done() && r.status && r.completedUs == at && r.wireFrameClosed);
  CHECK(r.origin == AckOrigin::Device && r.rxAccessible == 8 && r.rxTotal == 8);
  CHECK(f.health.healthSnapshot().totalSuccess == 1 &&
        f.health.healthSnapshot().totalFailures == 0);
  f.terminal();
}

void checkDiagnostic() {
  Fixture f;
  fragmented(f, FAST);
  const auto sample = f.sample();
  f.request = f.exchange.result().request;
  f.request.operationId++;
  f.request.publishMeasurement = false;
  f.request.context.startRegister = 0x0100;
  f.request.context.countOrWriteWord = 3;
  f.budget(500000);  // Explicit per-diagnostic device allowance.
  f.port.drained = false;
  const auto start = f.port.time;
  f.send(BLOCK_REQUEST);
  f.drain(start + FAST.drain);
  f.deliver(start + 450000, {BLOCK_REPLY});
  f.poll(start + 451750);
  const auto& r = f.exchange.result();
  CHECK(f.exchange.done() && r.status && r.rxAccessible == 11 && r.wordCount == 3);
  CHECK(r.words[0] == 754 && r.words[1] == 751 && r.words[2] == 300);
  CHECK(f.health.healthSnapshot().totalSuccess == 2);
  CHECK(f.sample().raw.sampleGeneration == sample.raw.sampleGeneration);
  CHECK(f.sample().usable.measurement.completedMs == sample.usable.measurement.completedMs);
  for (auto kind : {MeasurementKind::ProcessedDistance, MeasurementKind::Temperature}) {
    CHECK(f.cache.snapshot(kind, 1000, 1000).raw.freshness == Freshness::NeverSampled);
  }
  f.terminal();
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    for (const auto& timeline : {FAST, SLOW}) {
      Fixture f(timeline, 7, model);
      fragmented(f, timeline);
    }
  }
  checkExtraBytes(7, {0});
  checkExtraBytes(8, {0});
  checkExtraBytes(16, REALTIME_REPLY);
  checkProcessed();
  checkPollAcrossClosure();
  checkQueuedBatches();
  checkTxFailures();
  checkDeadline(FAST.deadline);
  checkDeadline(FAST.deadline + 10000);
  checkReadErrors();
  checkWireGap(FAST, 750);
  checkWireGap(SLOW, 1562);
  checkForeignThenReply();
  checkSplitFrames();
  checkBufferedEchoThenAck();
  checkErroredEcho();
  checkWrite(AckOrigin::Device, true, ADDRESS_WRITE);
  checkWrite(AckOrigin::Device, true, WRITE_EXCEPTION);
  checkWrite(AckOrigin::LocalEcho, true, ADDRESS_WRITE);
  checkWrite(AckOrigin::Unknown, true, ADDRESS_WRITE);
  checkWrite(AckOrigin::Device, false, ADDRESS_WRITE);
  checkWrite(AckOrigin::Device, true, {1, 6, 2, 0, 0, 6, 0x08, 0x70});
  checkDiagnostic();
}
