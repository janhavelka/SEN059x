/** @file Configuration.h
 *  @brief Passive commissioning evidence for one explicitly selected RS485 sensor.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Settings.h"
#include "WriteCodec.h"

namespace SEN059x {

/// Address and 8N1 bit rate used by the owner on this binding's bus.
struct Endpoint {
  std::uint8_t address = 0;  ///< Addressed device, including for later vendor-write observation.
  std::uint32_t bps = 0;     ///< Serial bit rate in bits per second.
};

/// Addressed FC06 with independent verification, or isolated FC06 to 255 with local TX result.
enum class WriteScope : std::uint8_t { Addressed, VendorSendOnly };
/// Owner evidence for one FC06. Zero means the owner can exclude every transmitted byte.
enum class TxProgress : std::uint8_t { Zero, Possible, Partial, Complete };
/// Owner qualification of received FC06 bytes after settling frame boundaries and adapter echo.
enum class AckOrigin : std::uint8_t { Unknown, LocalEcho, Device };

/// One successfully decoded, independently addressed FC03.
struct ConfigurationObservation {
  SettingReadback value;          ///< Exact word, selector and device identity.
  Endpoint endpoint;              ///< Endpoint on which the owner received the reply.
  std::uint32_t completedMs = 0;  ///< Owner's monotonic millisecond timestamp, including zero.
};

/// Retained setting history. A historical word keeps its original endpoint and time.
struct ConfigurationSnapshot {
  std::optional<ConfigurationObservation> observed;  ///< Empty until the first attributable FC03.
  bool trusted = false;  ///< Whether the observation represents current knowledge of this word.
};

/// Evidence for the latest prepared write. Copy by value to retain it across subsequent writes.
struct ConfigurationOperation {
  Setting setting{};               ///< Zero until a write is prepared.
  SettingValue desired{};          ///< Requested unit and register word.
  Endpoint original, intended;     ///< Addressed endpoints before and after the requested change.
  ConfigurationSnapshot before;    ///< Register history at the time the owner prepared the write.
  WriteScope scope{};              ///< Determines the destination and availability of verification.
  TxProgress tx{};                 ///< Progress of this single FC06.
  std::optional<Status> txStatus;  ///< Most recent owner send result, empty before reporting TX.
  std::optional<Status> ackStatus;  ///< FC06 parser result or owner's missing-reply diagnostic.
  WriteResponseError ackError{};    ///< Structured shape diagnostic when ACK bytes were supplied.
  AckOrigin ackOrigin{};            ///< Qualification independent of ACK shape.
  std::optional<ConfigurationObservation>
      readback;                      ///< Actual verification word, even on mismatch.
  std::optional<Status> readStatus;  ///< Underlying attributable FC03 or transport result.
  ReadResponseError readError{};     ///< Structured diagnostic when verification bytes were parsed.
  std::optional<Status> primary;  ///< Send-only TX, refusal, interruption or verification result.
  std::optional<Status> cleanup;  ///< Independent owner cleanup result.
  bool verified = false;          ///< True only after a matching addressed verification FC03.
  bool interrupted = false;       ///< Power was interrupted while this write was pending.
};

class Configuration;

/// Copy with the owner's exchange record. Valid only on its originating live Configuration.
class ConfigurationToken {
 private:
  friend class Configuration;
  const Configuration* owner_ = nullptr;
  std::uint64_t sequence_ = 0;
};

/// Caller-owned state for one device. Serialize calls, keep one exchange on the bus, and settle
/// late wire traffic before reuse. All builders only encode bytes. The owner controls TX, UART
/// changes, admission, clocks and recovery. No method performs bus I/O.
class Configuration {
 public:
  Configuration() = default;
  Configuration(const Configuration&) = delete;
  Configuration& operator=(const Configuration&) = delete;

  /// Select a device and supported 8N1 endpoint, clear all evidence and retire earlier tokens.
  /// Use DFRobotIsolated only on an isolated link. Invalid arguments preserve existing state.
  Status bind(DeviceModel model, Endpoint endpoint,
              AddressPolicy policy = AddressPolicy::StandardUnicast);
  /// Last attributable endpoint, or the selected starting endpoint before any addressed FC03.
  Endpoint endpoint() const { return endpoint_; }
  /// Endpoint proposed by a possibly transmitted address or baud write, until an attributable
  /// read establishes the responding endpoint. Survives unrelated previews and refusals.
  std::optional<Endpoint> possibleEndpoint() const { return possibleEndpoint_; }
  /// True after an attributable FC03 until possible address/baud TX or interruption.
  bool endpointTrusted() const { return endpointTrusted_; }
  /// Copy this register's history. An unknown selector returns empty history.
  ConfigurationSnapshot snapshot(Setting setting) const;
  /// Borrow evidence until the next mutation. A separate explicit read preserves this record.
  const ConfigurationOperation& operation() const { return operation_; }

  /// Prepare or preview one checked FC06, retaining its plan; no TX is assumed. A successful
  /// call supersedes earlier exchange tokens. Supply disjoint tx/out storage and capacity in
  /// bytes. Returns eight, or zero with all outputs and state preserved.
  std::size_t buildWrite(Setting setting, SettingValue value, WriteScope scope, std::uint8_t* tx,
                         std::size_t capacity, ConfigurationToken& out);
  /// Report progress immediately as a write becomes possible. Progress may advance before ACK or
  /// verification. While TX is unsettled, proof of zero bytes can resolve Possible and restore
  /// prior trust. Partial already establishes bytes were sent. Zero, Complete or a failure
  /// settles TX.
  /// Returns the supplied status on acceptance. The owner may still verify after possible TX.
  Status recordTx(const ConfigurationToken& write, TxProgress progress, Status result);
  /// Parse the complete FC06 rx frame (length in bytes) at the original address. Returns and
  /// retains its wire status separately from the owner's origin qualification. Baud ACK rate is
  /// unspecified. An echo-shaped reply alone never verifies a change.
  Status recordAck(const ConfigurationToken& write, const std::uint8_t* rx, std::size_t length,
                   AckOrigin origin);
  /// Record a missing addressed ACK and return the supplied non-OK owner diagnostic.
  Status missAck(const ConfigurationToken& write, Status failure);
  /// End a pending write without further verification, e.g. at a qualified device exception or
  /// owner cancellation. Report any possible TX first. Returns and retains the non-OK failure.
  Status finishWrite(const ConfigurationToken& write, Status failure);

  /// Encode an independent FC03 at the current endpoint or the still-uncertain intended endpoint.
  /// The owner identifies this physical binding. Supply disjoint tx/out storage and byte capacity.
  /// Returns eight on success, superseding an earlier independent read. Returns zero when
  /// verification is in flight, the endpoint is unrelated, or arguments are invalid.
  std::size_t buildRead(Endpoint endpoint, Setting setting, std::uint8_t* tx, std::size_t capacity,
                        ConfigurationToken& out);
  /// Encode an addressed write's verification FC03 at its intended address and bps for the same
  /// register. Supply disjoint tx/out storage and byte capacity. Eight on success, zero on
  /// rejection. Vendor sends have no verification exchange attached to their operation result.
  std::size_t buildVerification(const ConfigurationToken& write, std::uint8_t* tx,
                                std::size_t capacity, ConfigurationToken& out);
  /// Supply one correlated, frame-closed FC03 reply (length bytes) and the actual serial endpoint.
  /// A stale token or incorrect endpoint returns INVALID_CONFIG without consuming the active read.
  /// A valid word or exact FC03 exception establishes the responding endpoint. Only a word reply
  /// updates the setting observation at completedMs (owner monotonic milliseconds). A response
  /// establishing a different endpoint aborts an unsent write plan for the previous endpoint.
  /// Verification may return CONFIG_MISMATCH; a failed endpoint verification may return LINK_LOST.
  Status completeRead(const ConfigurationToken& read, Endpoint actualEndpoint,
                      const std::uint8_t* rx, std::size_t length, std::uint32_t completedMs);
  /// Retire a current read on a non-OK owner result such as TIMEOUT or IO. Verification retains the
  /// failure even when the primary result is LINK_LOST. Observation history survives the failure.
  Status failRead(const ConfigurationToken& read, Status failure);
  /// Record the owner's final cleanup once for this write. Returns the supplied status and
  /// preserves the primary result, even if cleanup failed.
  Status recordCleanup(const ConfigurationToken& write, Status result);
  /// On power interruption, invalidate live trust and pending transfers. Retain observations and
  /// completed results. An interrupted pending write records ABORTED or LINK_LOST.
  void interrupt();

 private:
  bool current(const ConfigurationToken& token, std::uint64_t id) const;
  bool isReadEndpoint(Endpoint endpoint) const;
  std::uint64_t issue(ConfigurationToken& token);
  bool awaitingAck(const ConfigurationToken& write) const;
  Status finishRead(Status status);
  std::size_t prepareRead(Endpoint endpoint, Setting setting, std::uint8_t* tx,
                          std::size_t capacity, ConfigurationToken& out);
  std::array<ConfigurationSnapshot, 8> settings_{};
  DeviceModel model_{};
  AddressPolicy policy_{};
  Endpoint endpoint_{};
  std::optional<Endpoint> possibleEndpoint_;  // Survives unrelated previews and refusals.
  bool endpointTrusted_ = false;
  ConfigurationOperation operation_{};
  RequestContext writeContext_{};
  SettingRequest readRequest_;
  Endpoint readEndpoint_{};
  std::uint64_t sequence_ = 0, writeId_ = 0, readId_ = 0;
  bool verification_ = false;
  bool txSettled_ = false;
  // Trust displaced by an unsettled Possible report, in case the driver later proves zero TX.
  bool settingTrustedBeforePossible_ = false;
  bool endpointTrustedBeforePossible_ = false;
  std::optional<Endpoint> candidateBeforePossible_;
};

}  // namespace SEN059x
