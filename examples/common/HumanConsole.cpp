#include "HumanConsole.h"

#include <algorithm>

namespace SEN059x::example {
namespace {
bool separator(char byte) {
  return byte == ' ' || byte == '\t';
}
}  // namespace

HumanConsole::HumanConsole(ConsoleSink& sink, ConsoleIdentity identity, Scheduler& owner)
    : sink_(sink), identity_(identity), owner_(owner) {
  owner_.maintenanceOff();
  help();
  prompt();
}

void HumanConsole::append(std::string_view text) {
  if (text.size() > output_.size() - used_) {
    overflow_ = true;
    return;
  }
  std::copy(text.begin(), text.end(), output_.begin() + used_);
  used_ += text.size();
}

void HumanConsole::field(std::string_view text) {
  if (color_) {
    append("\x1b[36m");
  }
  append(text);
  if (color_) {
    append("\x1b[0m");
  }
}

void HumanConsole::info(std::string_view text) {
  field("[I]");
  append(" ");
  append(text);
  append("\n");
}

void HumanConsole::error(std::string_view text) {
  field("[E]");
  append(" ");
  append(text);
  append("\n");
}

void HumanConsole::prompt() {
  append("> ");
  if (overflow_) {
    used_ = 0;
    overflow_ = false;
    error("Response exceeds console capacity");
    append("> ");
  }
}

void HumanConsole::finishLine() {
  if (poisoned_) {
    error("Invalid or oversized input line");
  } else if (tokens_ != 0) {
    line_[length_] = '\0';
    for (std::size_t i = 0; i < length_; ++i) {
      if (separator(line_[i])) {
        line_[i] = '\0';
      }
    }
    std::array<const char*, 10> words{};
    for (std::size_t i = 0; i < tokens_; ++i) {
      words[i] = line_.data() + starts_[i];
    }
    dispatch(words.data(), tokens_);
  }
  length_ = tokens_ = 0;
  poisoned_ = false;
  prompt();
}

bool HumanConsole::feed(std::uint8_t byte) {
  if (afterCr_ && byte == '\n') {
    afterCr_ = false;
    return true;
  }
  if (outputPending()) {
    return false;
  }
  afterCr_ = byte == '\r';
  if (afterCr_ || byte == '\n') {
    finishLine();
  } else if (!poisoned_) {
    if (byte == '\b' || byte == 0x7f) {
      if (length_ > 0) {
        --length_;
        if (tokens_ != 0 && starts_[tokens_ - 1] == length_) {
          --tokens_;
        }
      }
    } else if (length_ == line_.size() - 1 || (byte < ' ' && byte != '\t') || byte > '~') {
      poisoned_ = true;
    } else {
      const char ch = static_cast<char>(byte);
      if (!separator(ch)) {
        if (length_ == 0 || separator(line_[length_ - 1])) {
          if (tokens_ == starts_.size()) {
            poisoned_ = true;
            return true;
          }
          starts_[tokens_++] = length_;
        }
        if (length_ - starts_[tokens_ - 1] == 64) {
          poisoned_ = true;
          return true;
        }
      }
      line_[length_++] = ch;
    }
  }
  return true;
}

std::uint64_t HumanConsole::service() {
  const auto wake = serviceJob(owner_.service());
  if (terminalPending_ && sent_ == used_) {
    terminalPending_ = false;
    append("\n");
    showResult(*retained_);
    prompt();
  }
  if (sent_ < used_) {
    const auto count = std::min<std::size_t>(64, used_ - sent_);
    sent_ += sink_.write(output_.data() + sent_, count);
    if (sent_ == used_) {
      sent_ = used_ = 0;
    }
  }
  return wake;
}

}  // namespace SEN059x::example
