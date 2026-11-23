/** @file DeviceHealth.h
 *  @brief Passive communication health for one explicitly selected sensor.
 */
#pragma once

#include <cstdint>

#include "Types.h"

namespace SEN059x {

/// Communication state derived from completed, attributable exchange outcomes.
enum class DriverState : std::uint8_t {
  UNINIT,    ///< No exchange outcome on this binding.
  READY,     ///< The latest tracked attempt received a validated device reply.
  DEGRADED,  ///< The failure streak is below the offline threshold.
  OFFLINE,   ///< The failure streak has reached the offline threshold.
};

/// Return the static uppercase state name, or "UNKNOWN" for an unrecognized value.
const char* driverStateName(DriverState state);

/// Copy of health for the current binding. Each timestamp uses the owner's monotonic ms clock.
struct HealthSnapshot {
  DriverState state = DriverState::UNINIT;  ///< Latest communication state.
  bool initialized = false;                 ///< At least one attributable reply on this binding.
  std::uint8_t consecutiveFailures = 0;     ///< Saturating current failure streak.
  std::uint32_t totalFailures = 0;          ///< Saturating failures since bind or totals reset.
  std::uint32_t totalSuccess = 0;           ///< Saturating replies since bind or totals reset.
  std::uint32_t lastOkMs = 0;               ///< Latest reply time, when lastOkTimeValid.
  std::uint32_t lastErrorMs = 0;            ///< Latest failure time, when lastErrorTimeValid.
  bool lastOkTimeValid = false;             ///< A known reply time can be zero.
  bool lastErrorTimeValid = false;          ///< A known failure time can be zero.
  Status lastError;                         ///< Latest communication failure, retained on recovery.
};

class DeviceHealth;

/// Retain by value with a transmitted exchange. Valid only within the originating object's life.
class HealthAttempt {
 private:
  friend class DeviceHealth;
  const DeviceHealth* owner_ = nullptr;
  std::uint64_t sequence_ = 0;
};

/// One caller-owned health record per device binding. Serialize access and qualify events at the
/// bus owner, which retains the actual endpoint and request. All methods perform zero bus I/O.
class DeviceHealth {
 public:
  DeviceHealth() = default;
  DeviceHealth(const DeviceHealth&) = delete;
  DeviceHealth& operator=(const DeviceHealth&) = delete;

  /**
   * @brief Start a fresh binding, clearing health and retiring any outstanding token.
   * Call on every binding change, including a serial port or baud change.
   * @param model Explicit SKU selected by the owner.
   * @param address Address under the specified address policy.
   * @param offlineThreshold Consecutive failures to reach OFFLINE, from 1 to 255.
   * @param addressPolicy Use DFRobotIsolated only on an isolated link.
   * @return OK on a new binding, or INVALID_CONFIG with prior state preserved.
   */
  Status bind(DeviceModel model, std::uint8_t address, std::uint8_t offlineThreshold = 5,
              AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

  /// Drop the binding, pending token and all health history. Returns to UNINIT.
  void unbind();

  /**
   * @brief Identify one addressed attempt sent by the owner, including a transmitted retry.
   * @param out Caller storage for the attempt identity. Copies can settle it only once.
   * @return True on success. If unbound, busy or out of identities, preserve out and state.
   */
  bool beginAttempt(HealthAttempt& out);

  /**
   * @brief Record a correlated, complete and validated device reply on this attempt.
   * Includes an addressed exception, an unusable measurement or a mismatching readback.
   * For FC06 the owner must exclude its own TX echo. Ignore foreign bytes and keep waiting.
   * @param attempt Identity from beginAttempt for the responding device.
   * @param completedMs Owner's monotonic millisecond tick at completion, allowed to wrap.
   * @param timeValid Whether completedMs is known, including zero.
   * @return True if the attempt was current and consumed. Otherwise preserve all state.
   */
  bool recordReply(const HealthAttempt& attempt, std::uint32_t completedMs, bool timeValid);

  /**
   * @brief Settle an attempted exchange with one attributable communication failure.
   * @param attempt Identity from beginAttempt for the attempted device.
   * @param failure Underlying TIMEOUT, IO, CRC_ERROR or FRAME_ERROR. Its msg has static lifetime.
   * @param completedMs Owner's monotonic millisecond tick at completion, allowed to wrap.
   * @param timeValid Whether completedMs is known, including zero.
   * @return True on a current token and supported failure. Rejection preserves the attempt.
   */
  bool recordFailure(const HealthAttempt& attempt, Status failure, std::uint32_t completedMs,
                     bool timeValid);

  /// Retire an attempt with no attributable outcome. Return false for a noncurrent token.
  bool discard(const HealthAttempt& attempt);
  /// Return the current binding's communication state.
  DriverState state() const { return snapshot_.state; }
  /// True after first contact while READY or DEGRADED.
  bool isOnline() const;
  /// Copy all communication diagnostics without changing the outstanding attempt.
  HealthSnapshot healthSnapshot() const { return snapshot_; }
  /// Clear only totalSuccess and totalFailures. Retain streak, times, error and pending token.
  void resetStatistics();

 private:
  HealthSnapshot snapshot_;
  std::uint64_t sequence_ = 0;         // Never reused, even across unbind and rebind.
  std::uint8_t offlineThreshold_ = 0;  // Zero when unbound.
  bool active_ = false;
};

}  // namespace SEN059x
