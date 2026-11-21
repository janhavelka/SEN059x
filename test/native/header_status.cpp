#include <SEN059x/Status.h>

using SEN059x::Err;
using SEN059x::ReadResponseError;
using SEN059x::Status;

static_assert(static_cast<std::uint16_t>(Err::OK) == 0);
static_assert(Status{}.ok());

static_assert(static_cast<std::uint8_t>(ReadResponseError::NONE) == 0);
constexpr ReadResponseError readErrors[] = {
    ReadResponseError::NONE,
    ReadResponseError::INVALID_ARGUMENT,
    ReadResponseError::FRAME_TOO_SHORT,
    ReadResponseError::CRC_MISMATCH,
    ReadResponseError::UNEXPECTED_ADDRESS,
    ReadResponseError::UNEXPECTED_FUNCTION,
    ReadResponseError::EXCEPTION_RESPONSE,
    ReadResponseError::BYTE_COUNT_MISMATCH,
    ReadResponseError::LENGTH_MISMATCH,
};
static_assert(readErrors[0] == ReadResponseError::NONE);
