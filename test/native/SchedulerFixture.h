#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include "Scheduler.h"

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

namespace scheduler_test {
using namespace SEN059x;
using namespace SEN059x::example;

struct Port : SchedulerPort {
  std::uint64_t time = 0;
  bool drained = true;
  bool transmitting = false;
  std::uint32_t baud = 0;
  std::uint32_t unsupportedBaud = 0;
  std::size_t writeLimit = 8;
  Status drainStatus;
  Status configureStatus;
  bool isolationVerified = false;
  unsigned reads = 0;
  std::vector<std::vector<std::uint8_t>> writes;
  std::vector<std::uint8_t> pending;
  AckOrigin origin{};
  bool frameStarted = false;
  bool frameClosed = false;

  std::uint64_t nowUs() const override { return time; }
  bool supportsBaud(std::uint32_t rate) const override {
    Baud code;
    return rate != unsupportedBaud && valueToBaud(rate, code);
  }
  Status configure8N1(std::uint32_t nextBaud) override {
    CHECK(!transmitting);
    if (configureStatus) {
      baud = nextBaud;
    }
    return configureStatus;
  }
  void setTransmit(bool enabled) override {
    CHECK(enabled != transmitting);
    transmitting = enabled;
  }
  std::size_t write(const std::uint8_t* src, std::size_t length) override {
    CHECK(transmitting && baud != 0);
    writes.emplace_back(src, src + std::min(length, writeLimit));
    return writes.back().size();
  }
  Status txDrained(bool& out) override {
    CHECK(transmitting);
    out = drained;
    return drainStatus;
  }
  void deliver(std::vector<std::uint8_t> bytes, AckOrigin source = AckOrigin::Device,
               bool framed = true) {
    CHECK(pending.empty());
    pending = std::move(bytes);
    origin = source;
    frameStarted = frameClosed = framed;
  }
  RxBatch read(std::uint8_t* dst, std::size_t capacity) override {
    CHECK(!transmitting);
    ++reads;
    RxBatch batch;
    batch.total = pending.size();
    batch.copied = std::min(capacity, batch.total);
    std::copy_n(pending.begin(), batch.copied, dst);
    batch.origin = origin;
    batch.wireFrameStarted = frameStarted && !pending.empty();
    batch.wireFrameClosed = frameClosed && !pending.empty();
    pending.clear();
    return batch;
  }
  bool confirmIsolation(const ExchangeRequest&) override {
    if (isolationVerified) {
      transmitting = false;
      pending.clear();
      isolationVerified = false;
      return true;
    }
    return false;
  }
};

struct Timeline {
  using Bytes = std::vector<std::uint8_t>;
  Port port;
  Scheduler owner{port};

  std::uint64_t poll(std::uint64_t time) {
    CHECK(port.time <= time);
    port.time = time;
    return owner.service();
  }

  std::uint64_t submit(std::size_t slot, MeasurementKind kind, std::uint64_t id, std::uint32_t step,
                       std::uint64_t deadline = 1000000, bool retry = false) {
    std::uint64_t ticket = 0;
    CHECK(owner.submit(slot, kind, id, step, deadline, retry, ticket));
    return ticket;
  }

  void send(std::uint64_t time, const Bytes& expected, std::uint32_t baud = 115200) {
    const auto before = port.writes.size();
    poll(time);
    CHECK(port.writes.size() == before + 1 && port.writes.back() == expected);
    CHECK(port.transmitting && port.baud == baud);
  }

  void drain(std::uint64_t time) {
    CHECK(port.transmitting);
    poll(time);
    CHECK(!port.transmitting);
  }

  void reply(std::uint64_t time, const Bytes& bytes) {
    CHECK(!port.transmitting);
    port.deliver(bytes);
    poll(time);
  }

  ScheduledResult collect(std::uint64_t ticket) {
    ScheduledResult result{};
    CHECK(owner.take(ticket, result) && result.ticket == ticket);
    ScheduledResult duplicate{};
    CHECK(!owner.take(ticket, duplicate));
    return result;
  }
};
}  // namespace scheduler_test
