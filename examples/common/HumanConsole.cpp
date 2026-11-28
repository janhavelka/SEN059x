#include "HumanConsole.h"

#include <algorithm>

namespace SEN059x::example {
namespace {
bool separator(char byte) {
  return byte == ' ' || byte == '\t';
}
}  // namespace

HumanConsole::HumanConsole(ConsoleSink& sink, ConsoleIdentity identity, Scheduler& owner,
                           std::uint64_t bootId)
    : sink_(sink), identity_(identity), owner_(owner) {
  constexpr char digits[] = "0123456789abcdef";
  for (unsigned i = 0; i < 16; ++i) {
    boot_[15 - i] = digits[bootId & 15];
    bootId >>= 4;
  }
  owner_.maintenanceOff();
  help();
  prompt();
}

void HumanConsole::append(std::string_view text) {
  if (machine_)
    return;
  if (text.size() > 3072 - used_) {
    overflow_ = true;
    return;
  }
  std::copy(text.begin(), text.end(), output_.begin() + used_);
  used_ += text.size();
  if (!text.empty())
    humanLineOpen_ = text.back() != '\n';
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
  if (machine_) {
    replyStatus_ = {Err::INVALID_CONFIG, 0, ""};
    json_.value("message", text);
    return;
  }
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
    ++droppedOutput_;
    error("Response exceeds console capacity");
    append("> ");
  }
}

void HumanConsole::finishLine() {
  line_[length_] = '\0';
  for (std::size_t i = 0; i < length_; ++i) {
    if (separator(line_[i]))
      line_[i] = '\0';
  }
  std::array<const char*, 10> words{};
  for (std::size_t i = 0; i < tokens_; ++i)
    words[i] = line_.data() + starts_[i];

  if (length_ && line_[0] == '@') {
    machineLine(words.data(), tokens_);
  } else {
    if (poisoned_) {
      error("Invalid or oversized input line");
    } else if (tokens_) {
      dispatch(words.data(), tokens_);
    }
    prompt();
  }
  length_ = tokens_ = 0;
  poisoned_ = false;
}

bool HumanConsole::canFeed(std::uint8_t byte) const {
  if (afterCr_ && byte == '\n')
    return true;
  if (!outputPending())
    return true;
  if (terminalPending_)
    return false;
  const bool machineLine = length_ ? line_[0] == '@' : byte == '@';
  return machineLine && output_.size() - (used_ - sent_) >= MAX_RECORD + 1;
}

bool HumanConsole::feed(std::uint8_t byte) {
  if (afterCr_ && byte == '\n') {
    afterCr_ = false;
    return true;
  }
  if (!canFeed(byte)) {
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
    if (retained_->req) {
      beginRecord();
      terminalRecord(*retained_);
      finishRecord(&*retained_);
    } else {
      append("\n");
      showResult(*retained_);
      prompt();
    }
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
