#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string_view>

namespace SEN059x::example {

// Writes one JSON value directly into a caller-owned record. A failed record is never flushed.
class JsonWriter {
 public:
  void reset(char* data, std::size_t capacity) {
    data_ = data;
    capacity_ = capacity;
    used_ = 0;
    failed_ = comma_ = false;
  }
  bool ok() const { return !failed_; }
  std::size_t size() const { return used_; }

  void object() {
    separator();
    put("{");
    comma_ = false;
  }
  void array() {
    separator();
    put("[");
    comma_ = false;
  }
  void endObject() {
    put("}");
    comma_ = true;
  }
  void endArray() {
    put("]");
    comma_ = true;
  }
  void key(std::string_view name) {
    str(name);
    put(":");
    comma_ = false;
  }
  void null() {
    separator();
    put("null");
  }
  void boolean(bool value) {
    separator();
    put(value ? "true" : "false");
  }
  template <typename Integer>
  void num(Integer value) {
    separator();
    digits(value);
  }
  void tenths(unsigned value) {
    num(value / 10);
    put(".");
    digits(value % 10);
  }
  void str(std::string_view value);
  void value(std::string_view name, std::string_view v) {
    key(name);
    str(v);
  }
  void flag(std::string_view name, bool v) {
    key(name);
    boolean(v);
  }
  template <typename Integer>
  void integer(std::string_view name, Integer v) {
    key(name);
    num(v);
  }

 private:
  template <typename Integer>
  void digits(Integer value) {
    char bytes[24];
    const auto result = std::to_chars(bytes, bytes + sizeof(bytes), value);
    put({bytes, static_cast<std::size_t>(result.ptr - bytes)});
  }
  void separator() {
    if (comma_)
      put(",");
    comma_ = true;
  }
  void put(std::string_view s) {
    if (s.size() > capacity_ - used_) {
      failed_ = true;
      return;
    }
    std::copy(s.begin(), s.end(), data_ + used_);
    used_ += s.size();
  }
  char* data_ = nullptr;
  std::size_t capacity_ = 0;
  std::size_t used_ = 0;
  bool failed_ = false;
  bool comma_ = false;
};

}  // namespace SEN059x::example
