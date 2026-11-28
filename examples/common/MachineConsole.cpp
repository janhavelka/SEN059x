#include <algorithm>
#include <charconv>

#include "HumanConsole.h"

namespace SEN059x::example {
namespace {
constexpr std::string_view PREFIX = "SEN059X ";

bool validNonce(std::string_view nonce) {
  if (nonce.empty() || nonce.size() > 32)
    return false;
  return std::all_of(nonce.begin(), nonce.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '-';
  });
}

// Status messages come from the adapter and have static lifetime. Bound the scan and record.
std::string_view message(Status status) {
  std::size_t n = 0;
  if (status.msg) {
    while (n < 64 && status.msg[n])
      ++n;
  }
  return {status.msg ? status.msg : "", n};
}
}  // namespace

void JsonWriter::str(std::string_view s) {
  separator();
  put("\"");
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < s.size(); ++i) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c == '"' || c == '\\') {
      put("\\");
      put(s.substr(i, 1));
    } else if (c < 32) {
      const char escaped[] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
      put({escaped, sizeof(escaped)});
    } else if (c < 128) {
      put(s.substr(i, 1));
    } else {
      // Copy complete UTF-8 scalars. A truncated or malformed adapter message stays valid JSON.
      std::size_t n = c >= 0xc2 && c <= 0xdf   ? 2
                      : c >= 0xe0 && c <= 0xef ? 3
                      : c >= 0xf0 && c <= 0xf4 ? 4
                                               : 0;
      bool valid = n && n <= s.size() - i;
      for (std::size_t j = 1; valid && j < n; ++j) {
        const auto next = static_cast<unsigned char>(s[i + j]);
        valid = next >= 0x80 && next <= 0xbf;
        if (j == 1)
          valid = valid && !(c == 0xe0 && next < 0xa0) && !(c == 0xed && next >= 0xa0) &&
                  !(c == 0xf0 && next < 0x90) && !(c == 0xf4 && next > 0x8f);
      }
      if (valid) {
        put(s.substr(i, n));
        i += n - 1;
      } else {
        put("\\ufffd");
      }
    }
  }
  put("\"");
}

void HumanConsole::beginRecord() {
  // Reclaim a partially written prefix without changing the order of pending records.
  if (sent_) {
    std::move(output_.begin() + sent_, output_.begin() + used_, output_.begin());
    used_ -= sent_;
    sent_ = 0;
  }
  if (humanLineOpen_)
    output_[used_++] = '\n';
  humanLineOpen_ = false;
  std::copy(PREFIX.begin(), PREFIX.end(), output_.begin() + used_);
  used_ += PREFIX.size();
  json_.reset(output_.data() + used_, MAX_RECORD - PREFIX.size() - 1);
  json_.object();
  json_.key("data");
  json_.object();
}

void HumanConsole::finishRecord(const Job* terminal) {
  if (!json_.ok()) {
    // Never send partial JSON. Preserve the correlation and the effects of admitted work.
    json_.reset(output_.data() + used_, MAX_RECORD - PREFIX.size() - 1);
    json_.object();
    json_.key("data");
    json_.object();
    json_.flag("projectionOverflow", true);
    if (terminal) {
      // The full result is still retained locally. An incomplete projection cannot prove success.
      json_.value("disposition", terminal->outcome.code == Err::ABORTED   ? "Cancelled"
                                 : terminal->outcome.code == Err::TIMEOUT ? "Timeout"
                                                                          : "Failed");
      json_.integer("transfers", terminal->transfers);
      json_.integer("attempts", terminal->transfers);
      json_.integer("elapsedMs", (terminal->endedUs - terminal->startedUs) / 1000);
      json_.value("cleanupCode", errToString(terminal->last.cleanup.code));
      json_.integer("cleanupDetail", terminal->last.cleanup.detail);
      if (const auto& write = terminal->last.write) {
        json_.flag("effectUncertain", write->tx != TxProgress::Zero && !write->readback);
        json_.flag("verified", write->verified);
      }
      envelope(terminal->nonce.data(), terminal->req, terminal->cmd, terminal->id,
               &terminal->outcome);
    } else {
      const Status tooLarge{Err::INVALID_CONFIG, 0, "Response exceeds record capacity"};
      envelope(nonce_.data(), request_, command_, admitted_, admitted_ ? nullptr : &tooLarge);
    }
  }
  used_ += json_.size();
  output_[used_++] = '\n';
}

void HumanConsole::jsonStatus(Status status) {
  json_.object();
  json_.value("code", errToString(status.code));
  json_.integer("detail", status.detail);
  json_.value("message", message(status));
  json_.endObject();
}

void HumanConsole::envelope(std::string_view nonce, std::uint32_t req, std::string_view cmd,
                            std::uint64_t op, const Status* status) {
  if (status && !message(*status).empty())
    json_.value("message", message(*status));
  if (status && status->code == Err::EXCEPTION)
    json_.value("exceptionName",
                modbusExceptionToString(static_cast<std::uint8_t>(status->detail)));
  json_.endObject();
  json_.integer("v", 1);
  json_.value("boot", boot_.data());
  json_.key("session");
  if (nonce.empty())
    json_.null();
  else
    json_.str(nonce);
  json_.key("req");
  if (req)
    json_.num(req);
  else
    json_.null();
  json_.value("cmd", cmd);
  json_.value("phase", status ? "done" : "accepted");
  json_.key("op");
  if (op)
    json_.num(op);
  else
    json_.null();
  json_.key("code");
  if (status)
    json_.str(errToString(status->code));
  else
    json_.null();
  json_.key("detail");
  if (status)
    json_.num(status->detail);
  else
    json_.null();
  json_.endObject();
}

void HumanConsole::terminalRecord(const Job& job) {
  jobData(job, true);
  envelope(job.nonce.data(), job.req, job.cmd, job.id, &job.outcome);
}

void HumanConsole::machineLine(const char* const* words, std::size_t count) {
  std::string_view id = words[0];
  // If the parser stopped inside the ID, its retained digits cannot identify a request.
  const bool incompleteId = poisoned_ && id.size() == length_;
  id.remove_prefix(1);
  std::uint32_t req = 0;
  const auto end = id.data() + id.size();
  const auto parsed = std::from_chars(id.data(), end, req);
  if (incompleteId || parsed.ec != std::errc{} || parsed.ptr != end || !req) {
    ++protocolErrors_;
    return;
  }
  const std::string_view verb = count >= 2 ? words[1] : "";
  const bool attach = !poisoned_ && req == 1 && verb == "session" && count == 3 &&
                      validNonce(words[2]) && std::string_view(words[2]) != nonce_.data();
  if (nonce_[0] && !attach && req <= highRequest_) {
    ++protocolErrors_;
    return;
  }
  if (attach) {
    owner_.maintenanceOff();
    const std::string_view nonce = words[2];
    std::copy(nonce.begin(), nonce.end(), nonce_.begin());
    nonce_[nonce.size()] = '\0';
  }
  if (nonce_[0])
    highRequest_ = req;
  machine_ = true;
  request_ = req;
  command_ = verb;
  admitted_ = 0;
  replyStatus_ = Ok();
  beginRecord();
  if (verb.empty()) {
    error("Invalid or oversized input line");
  } else if (attach) {
    identityData();
  } else if (verb == "session") {
    error("Expected @1 session followed by a new nonce");
  } else {
    dispatch(words + 1, count - 1);
  }
  envelope(nonce_.data(), req, command_, admitted_, admitted_ ? nullptr : &replyStatus_);
  finishRecord();
  machine_ = false;
}

}  // namespace SEN059x::example
