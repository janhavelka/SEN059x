// Host-side subset of FieldCore main eccdbdc's Rs485OwnerTransaction and
// Rs485Task::processResponse. All UART input and uptime come from the test.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace FieldCoreMain {

constexpr int kTxPin = 47;
constexpr int kRxPin = 48;
constexpr int kDeRePin = 21;
constexpr bool kDeActiveHigh = true;
constexpr std::uint32_t kRs485DefaultBaud = 9600;
constexpr std::uint16_t kRs485TransactionTxCapacity = 8;
constexpr std::uint16_t kRs485TransactionRxCapacity = 192;
constexpr std::uint8_t kRs485TransactionSuffixCapacity = 2;

// FieldCore's Deadline64 permits uptime rollover for intervals shorter than 2^63 ms.
constexpr bool dueOrPast(std::uint64_t nowMs, std::uint64_t dueMs) {
  return nowMs - dueMs < (std::uint64_t{1} << 63);
}

enum class Rs485Parity : std::uint8_t { None, Even, Odd };
struct Rs485SerialFormat {
  std::uint32_t baud{};
  std::uint8_t dataBits{8};
  Rs485Parity parity{Rs485Parity::None};
  std::uint8_t stopBits{1};
};

constexpr bool rs485SerialFormatValid(const Rs485SerialFormat& format) {
  const auto baud = format.baud;
  return (baud == 9600 || baud == 19200 || baud == 38400 || baud == 57600 || baud == 115200) &&
         format.dataBits == 8 && format.parity == Rs485Parity::None && format.stopBits == 1;
}

enum class Rs485CompletionMode : std::uint8_t { FixedLength, Suffix };
enum class Rs485TransportTerminalReason : std::uint8_t {
  Complete,
  ResponseTimeout,
  InterByteTimeout,
  BufferFull,
  BackendError,
  DeadlineExpired
};

struct Rs485BusTransaction {
  Rs485SerialFormat format{};
  std::uint8_t txBytes[kRs485TransactionTxCapacity]{};
  std::uint16_t txLength{};
  std::uint16_t rxCapacity{};
  std::uint32_t responseTimeoutMs{};
  std::uint32_t interByteTimeoutMs{};
  Rs485CompletionMode completionMode{Rs485CompletionMode::FixedLength};
  std::uint16_t expectedLength{};
  std::uint8_t suffixBytes[kRs485TransactionSuffixCapacity]{};
  std::uint8_t suffixLength{};
  bool discardBeforeTransmit{true};
};

constexpr bool rs485BusTransactionValid(const Rs485BusTransaction& tx) {
  if (!rs485SerialFormatValid(tx.format) || tx.txLength == 0 ||
      tx.txLength > kRs485TransactionTxCapacity || tx.rxCapacity > kRs485TransactionRxCapacity ||
      tx.responseTimeoutMs == 0 || tx.interByteTimeoutMs > tx.responseTimeoutMs) {
    return false;
  }
  if (tx.completionMode == Rs485CompletionMode::FixedLength) {
    return tx.expectedLength <= tx.rxCapacity;
  }
  return tx.completionMode == Rs485CompletionMode::Suffix && tx.suffixLength > 0 &&
         tx.suffixLength <= kRs485TransactionSuffixCapacity && tx.suffixLength <= tx.rxCapacity;
}

enum class ErrorCode : std::uint16_t {
  Ok = 0,
  InvalidArgument = 2,
  Timeout = 3,
  DeadlineExpired = 4,
  Unavailable = 8,
  Busy = 9,
  Unsupported = 10,
  InternalFault = 11,
  Rs485BadFrame = 600,
  Rs485ParserError = 601,
  Rs485RetryExhausted = 602,
  Rs485DeviceStale = 603,
  Rs485UnsupportedProfile = 604,
  Rs485ProtocolLibraryError = 605,
  Rs485NoCachedSnapshot = 606
};
struct ErrorDetail {
  ErrorCode code{ErrorCode::Ok};
  std::int32_t backendCode{};
};

struct Rs485BusObservation {
  Rs485TransportTerminalReason reason{Rs485TransportTerminalReason::BackendError};
  ErrorDetail error{};
  const std::uint8_t* rxBytes{};
  std::uint16_t rxLength{};
  std::uint16_t txCount{};
  std::uint16_t rxCount{};
  std::uint64_t completedUptimeMs{};
};

enum class Rs485ModuleAction : std::uint8_t { None, RequestTransaction, WaitUntil };
struct Rs485ModuleStep {
  Rs485ModuleAction action{Rs485ModuleAction::None};
  Rs485BusTransaction transaction{};
  std::uint64_t wakeUptimeMs{};
};

class Owner {
 public:
  // A successful dispatch models one completed write/flush at nowMs.
  bool dispatch(const Rs485ModuleStep& step, std::uint64_t nowMs, std::uint64_t expiresAtMs) {
    if (active_ || step.action != Rs485ModuleAction::RequestTransaction ||
        dueOrPast(nowMs, expiresAtMs) || !rs485BusTransactionValid(step.transaction)) {
      return false;
    }
    tx_ = step.transaction;
    active_ = true;
    length_ = 0;
    startedMs_ = nowMs;
    deadlineMs_ = expiresAtMs;
    ++transfers;
    if (tx_.txLength >= 2 && tx_.txBytes[1] == 3) {
      ++fc03Transfers;
    }
    if (tx_.txLength >= 2 && tx_.txBytes[1] == 6) {
      ++fc06Transfers;
    }
    return true;
  }

  // One nonblocking backend read. The caller consumes out synchronously on
  // true. Supply monotonically increasing uptime; (nullptr, 0) advances time.
  bool receive(const std::uint8_t* bytes, std::size_t available, std::uint64_t nowMs,
               Rs485BusObservation& out) {
    using Reason = Rs485TransportTerminalReason;
    if (!active_) {
      return false;
    }
    const auto finish = [&](Reason reason, ErrorCode error = ErrorCode::Ok) {
      out = {reason, {error, 0}, rx_, length_, tx_.txLength, length_, nowMs};
      active_ = false;
      return true;
    };
    if (dueOrPast(nowMs, deadlineMs_)) {
      return finish(Reason::DeadlineExpired, ErrorCode::DeadlineExpired);
    }
    if (bytes == nullptr && available != 0) {
      return finish(Reason::BackendError, ErrorCode::InvalidArgument);
    }
    if (tx_.completionMode == Rs485CompletionMode::FixedLength && tx_.expectedLength == 0) {
      return finish(Reason::Complete);
    }

    // While a TX prefix still matches, main allows space for both the echo
    // and the requested reply, bounded by the single shared RX buffer.
    const bool couldBeEcho = length_ <= tx_.txLength && std::memcmp(rx_, tx_.txBytes, length_) == 0;
    const std::size_t limit = std::min(
        sizeof(rx_), static_cast<std::size_t>(tx_.rxCapacity) + (couldBeEcho ? tx_.txLength : 0));
    const auto count = std::min(available, limit > length_ ? limit - length_ : 0);
    if (count != 0) {
      std::memcpy(rx_ + length_, bytes, count);
      length_ += static_cast<std::uint16_t>(count);
      if (length_ >= tx_.txLength && std::memcmp(rx_, tx_.txBytes, tx_.txLength) == 0) {
        length_ -= tx_.txLength;
        std::memmove(rx_, rx_ + tx_.txLength, length_);
      }
      lastByteMs_ = nowMs;
    }
    const bool partialEcho =
        length_ > 0 && length_ < tx_.txLength && std::memcmp(rx_, tx_.txBytes, length_) == 0;
    if (!partialEcho) {
      if (length_ > tx_.rxCapacity) {
        length_ = tx_.rxCapacity;
        return finish(Reason::BufferFull, ErrorCode::Rs485BadFrame);
      }
      const bool complete =
          tx_.completionMode == Rs485CompletionMode::FixedLength
              ? length_ >= tx_.expectedLength
              : length_ >= tx_.suffixLength && std::memcmp(rx_ + length_ - tx_.suffixLength,
                                                           tx_.suffixBytes, tx_.suffixLength) == 0;
      if (complete) {
        return finish(Reason::Complete);
      }
      if (length_ == tx_.rxCapacity) {
        return finish(Reason::BufferFull, ErrorCode::Rs485BadFrame);
      }
    }
    if (nowMs - startedMs_ >= tx_.responseTimeoutMs) {
      return finish(Reason::ResponseTimeout, ErrorCode::Timeout);
    }
    if (!partialEcho && length_ > 0 && tx_.interByteTimeoutMs > 0 &&
        nowMs - lastByteMs_ >= tx_.interByteTimeoutMs) {
      return finish(Reason::InterByteTimeout, ErrorCode::Timeout);
    }
    return false;
  }

  std::uint32_t transfers{};
  std::uint32_t fc03Transfers{};
  std::uint32_t fc06Transfers{};

 private:
  bool active_{};
  Rs485BusTransaction tx_{};
  std::uint8_t rx_[kRs485TransactionRxCapacity]{};
  std::uint16_t length_{};
  std::uint64_t startedMs_{}, deadlineMs_{}, lastByteMs_{};
};

}  // namespace FieldCoreMain
