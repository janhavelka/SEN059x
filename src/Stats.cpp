#include "SEN059x/Stats.h"

#include "../internal/SaturatingIncrement.h"

namespace SEN059x {

void Stats::recordRetry() {
  internal::saturatingIncrement(retryCount);
}

void Stats::recordAttemptError(Status result) {
  switch (result.code) {
    case Err::CRC_ERROR:
      internal::saturatingIncrement(crcErrors);
      break;
    case Err::TIMEOUT:
      internal::saturatingIncrement(timeouts);
      break;
    case Err::EXCEPTION:
      internal::saturatingIncrement(exceptions);
      break;
    case Err::FRAME_ERROR:
      internal::saturatingIncrement(frameErrors);
      break;
    default:
      break;
  }
}

void Stats::recordOperation(Status result, std::uint32_t completedMs) {
  lastError = result.code;
  if (result.ok()) {
    internal::saturatingIncrement(successCount);
    lastSuccessTimestampMs = completedMs;
    lastErrorDetail = 0;
    lastErrorMsg = "";
  } else {
    internal::saturatingIncrement(failCount);
    lastErrorTimestampMs = completedMs;
    lastErrorDetail = result.detail;
    lastErrorMsg = result.msg;
  }
}

void Stats::reset() {
  *this = {};
}

}  // namespace SEN059x
