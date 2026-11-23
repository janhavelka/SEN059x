/** @file Stats.h
 *  @brief Passive operation and attempt statistics for a caller-selected endpoint or bus.
 */
#pragma once

#include <cstdint>

#include "Status.h"

namespace SEN059x {

/// The owner selects the endpoint or bus covered by this instance and serializes access.
/// The reporting interval begins at construction or reset(). Counters saturate at UINT32_MAX.
struct Stats {
  std::uint32_t successCount = 0;  ///< Logical operations completed successfully.
  std::uint32_t failCount = 0;     ///< Logical operations completed with an error.
  std::uint32_t retryCount = 0;    ///< Extra attempts actually transmitted.
  std::uint32_t crcErrors = 0;     ///< Attempts ending with a CRC error.
  std::uint32_t timeouts = 0;      ///< Attempts ending at the response deadline.
  std::uint32_t exceptions = 0;    ///< Attempts ending with a validated Modbus exception.
  std::uint32_t frameErrors = 0;   ///< Attempts ending with a frame error.

  Err lastError = Err::OK;                   ///< Latest terminal operation code.
  std::int32_t lastErrorDetail = 0;          ///< Signed detail of that failure, zero on success.
  const char* lastErrorMsg = "";             ///< Static text of that failure, empty on success.
  std::uint32_t lastErrorTimestampMs = 0;    ///< Last failed completion, valid when failCount > 0.
  std::uint32_t lastSuccessTimestampMs = 0;  ///< Last success, valid when successCount > 0.

  /// Count one extra attempt when its transmission starts.
  void recordRetry();

  /// Count the underlying CRC_ERROR, TIMEOUT, EXCEPTION or FRAME_ERROR once per settled attempt.
  /// Supply the correlated attempt result after discarding foreign traffic. Other codes are
  /// ignored.
  /// @param result Final exchange diagnostic, including on attempts that will be retried.
  void recordAttemptError(Status result);

  /// Record one terminal logical operation and update its diagnostics. A success clears the last
  /// error code, detail and text. Each timestamp retains the latest completion of its own kind.
  /// @param result Terminal operation status. Its message must have static storage duration.
  /// @param completedMs Completion on the owner's monotonic clock in milliseconds, including zero
  /// and values after wrap. This call counts only the logical operation.
  void recordOperation(Status result, std::uint32_t completedMs);

  /// Clear all owner counters, diagnostics and timestamps to their construction values.
  void reset();
};

}  // namespace SEN059x
