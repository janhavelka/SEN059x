#pragma once

#include <limits>
#include <type_traits>

namespace SEN059x::internal {

template <typename T>
constexpr void saturatingIncrement(T& count) {
  static_assert(std::is_unsigned_v<T>);
  if (count < std::numeric_limits<T>::max()) {
    ++count;
  }
}

}  // namespace SEN059x::internal
