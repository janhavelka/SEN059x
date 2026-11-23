#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <SEN059x/Stats.h>

namespace {
using namespace SEN059x;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

constexpr auto MAX_COUNT = std::numeric_limits<std::uint32_t>::max();
constexpr Status TIMEOUT{Err::TIMEOUT, -19, "response timeout"};
constexpr Status EXCEPTION{Err::EXCEPTION, 0xE1, "Modbus exception"};
constexpr Status FRAME_ERROR{Err::FRAME_ERROR, 3, "short frame"};
constexpr Status CRC_ERROR{Err::CRC_ERROR, 2, "CRC mismatch"};

void checkEmpty(const Stats& stats) {
  CHECK(stats.successCount == 0 && stats.failCount == 0 && stats.retryCount == 0);
  CHECK(stats.crcErrors == 0 && stats.timeouts == 0 && stats.exceptions == 0 &&
        stats.frameErrors == 0);
  CHECK(stats.lastError == Err::OK && stats.lastErrorDetail == 0);
  CHECK(std::strcmp(stats.lastErrorMsg, "") == 0);
  CHECK(stats.lastErrorTimestampMs == 0 && stats.lastSuccessTimestampMs == 0);
}

void checkReportingInterval() {
  Stats stats;
  checkEmpty(stats);

  // One sent attempt times out, then a transmitted retry returns a valid word.
  stats.recordAttemptError(TIMEOUT);
  CHECK(stats.timeouts == 1 && stats.retryCount == 0 && stats.failCount == 0);
  CHECK(stats.lastError == Err::OK && stats.lastErrorTimestampMs == 0);
  stats.recordRetry();
  stats.recordOperation(Ok(), 0);
  CHECK(stats.successCount == 1 && stats.retryCount == 1 && stats.timeouts == 1);
  CHECK(stats.failCount == 0 && stats.lastSuccessTimestampMs == 0);

  stats.recordAttemptError(EXCEPTION);
  stats.recordOperation(EXCEPTION, MAX_COUNT);
  CHECK(stats.exceptions == 1 && stats.failCount == 1 && stats.successCount == 1);
  CHECK(stats.lastError == Err::EXCEPTION && stats.lastErrorDetail == 0xE1);
  CHECK(std::strcmp(stats.lastErrorMsg, "Modbus exception") == 0);
  CHECK(stats.lastErrorTimestampMs == MAX_COUNT && stats.lastSuccessTimestampMs == 0);

  stats.recordOperation(Ok(), 4);
  CHECK(stats.successCount == 2 && stats.failCount == 1 && stats.exceptions == 1);
  CHECK(stats.lastError == Err::OK && stats.lastErrorDetail == 0);
  CHECK(std::strcmp(stats.lastErrorMsg, "") == 0);
  CHECK(stats.lastErrorTimestampMs == MAX_COUNT && stats.lastSuccessTimestampMs == 4);

  stats.recordAttemptError(CRC_ERROR);
  CHECK(stats.crcErrors == 1 && stats.frameErrors == 0);
  stats.recordAttemptError(FRAME_ERROR);
  CHECK(stats.frameErrors == 1 && stats.crcErrors == 1);

  // A failed verification can have different wire, operation and cleanup statuses.
  // Only the underlying timeout classifies the attempt.
  constexpr Status linkLost{Err::LINK_LOST, -7, "new endpoint unreachable"};
  stats.recordAttemptError(TIMEOUT);
  stats.recordOperation(linkLost, 9);
  CHECK(stats.timeouts == 2 && stats.failCount == 2 && stats.successCount == 2);
  CHECK(stats.lastError == Err::LINK_LOST && stats.lastErrorDetail == -7);
  CHECK(std::strcmp(stats.lastErrorMsg, "new endpoint unreachable") == 0);
  CHECK(stats.lastErrorTimestampMs == 9 && stats.lastSuccessTimestampMs == 4);
  stats.recordAttemptError({Err::IO, -23, "UART error"});
  stats.recordAttemptError(Ok());
  CHECK(stats.crcErrors == 1 && stats.timeouts == 2 && stats.frameErrors == 1 &&
        stats.exceptions == 1);
  CHECK(stats.retryCount == 1 && stats.lastError == Err::LINK_LOST);

  stats.reset();
  checkEmpty(stats);
}

void checkSaturation() {
  Stats stats;
  stats.successCount = stats.failCount = stats.retryCount = MAX_COUNT - 1;
  stats.crcErrors = stats.timeouts = stats.exceptions = stats.frameErrors = MAX_COUNT - 1;
  for (std::uint32_t tick = 1; tick <= 2; ++tick) {
    stats.recordOperation(Ok(), tick);
    stats.recordOperation(CRC_ERROR, tick);
    stats.recordRetry();
    stats.recordAttemptError(CRC_ERROR);
    stats.recordAttemptError(TIMEOUT);
    stats.recordAttemptError(EXCEPTION);
    stats.recordAttemptError(FRAME_ERROR);
    CHECK(stats.successCount == MAX_COUNT && stats.failCount == MAX_COUNT &&
          stats.retryCount == MAX_COUNT);
    CHECK(stats.crcErrors == MAX_COUNT && stats.timeouts == MAX_COUNT &&
          stats.exceptions == MAX_COUNT && stats.frameErrors == MAX_COUNT);
    CHECK(stats.lastSuccessTimestampMs == tick && stats.lastErrorTimestampMs == tick);
  }
}

}  // namespace

int main() {
  checkReportingInterval();
  checkSaturation();
}
