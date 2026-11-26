#include "HumanConsoleFixture.h"

namespace {
using namespace console_test;
using namespace SEN059x;
using scheduler_test::Port;

void readDiagnostics() {
  TestTerminal<Port> t;
  t.flush();
  CHECK(contains(t.send("drv\n"), "No bus request history"));
  CHECK(t.port.writes.empty());
  CHECK(t.owner.bind(0, DeviceModel::SEN0592, 1, 115200));
  std::uint64_t ticket = 0;
  CHECK(t.owner.submit(0, MeasurementKind::Temperature, 91, 1, 12000, false, ticket));
  t.turn();
  t.port.time = 300;
  auto active = t.send("drv\n");
  CHECK(contains(active, "operation 91, address 1, function 0x03, register 0x0102, count 1"));
  CHECK(contains(active, "Started 0 us, deadline 12000 us, elapsed 300 us"));
  CHECK(contains(active, "Primary pending") && contains(active, "Cleanup pending"));
  CHECK(!contains(active, "TX accepted"));
  ScheduledResult result;
  CHECK(!t.owner.take(ticket, result));
  t.send("verbose on\n");
  CHECK(contains(t.send("drv\n"), "TX accepted 8 bytes, drained, RX observed 0 bytes"));
  t.port.deliver({1, 0x83, 2, 0xC0, 0xF1});
  t.port.time = 1000;
  t.turn();
  const auto failed = t.send("drv\n");
  CHECK(contains(failed, "Primary EXCEPTION detail 2"));
  CHECK(contains(failed, "exception 0x02 Illegal Data Address"));
  CHECK(contains(failed, "Cleanup OK detail 0"));
  CHECK(contains(failed, "elapsed 1000 us, completed 1000 us"));
  CHECK(contains(failed, "RX observed 5 bytes, last delivery 1000 us"));
  CHECK(t.owner.take(ticket, result) && result.status.code == Err::EXCEPTION);
  CHECK(!t.owner.take(ticket, result));
  t.port.time = 9000;
  CHECK(t.send("drv\n") == failed);
  CHECK(t.owner.physicalAttempts() == 1 && t.port.writes.size() == 1);
}

void sharedHostBaud() {
  TestTerminal<Port> t;
  t.flush();
  t.send("model SEN0591\nhost baud 115200\n");
  CHECK(t.owner.bind(1, DeviceModel::SEN0592, 2, 9600));
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitSettingRead(1, Setting::Address, 2, 0, 100000, ticket));
  t.turn();
  const auto host = t.send("host\n");
  CHECK(contains(host, "address 1, 115200 baud 8N1") && contains(host, "Host UART 9600 baud 8N1"));
  CHECK(t.port.baud == 9600 && t.port.writes.size() == 1);
}

void vendorCleanup() {
  TestTerminal<Port> t;
  t.flush();
  CHECK(t.owner.bind(0, DeviceModel::SEN0591, 1, 115200, AddressPolicy::DFRobotIsolated));
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitSettingRead(0, Setting::Address, 20, 0, 10000, ticket));
  t.turn();
  t.turn();
  t.port.deliver({1, 3, 2, 0, 1, 0x79, 0x84});
  t.turn();
  ScheduledResult result;
  CHECK(t.owner.take(ticket, result) && result.status);
  CHECK(t.owner.grantIsolatedMaintenance(0, 7));
  CHECK(t.owner.submitWrite(0, Setting::Address, {SettingUnit::Address, 5}, 7, 21, 0, 900, ticket,
                            WriteScope::VendorSendOnly));
  t.port.time = 100;
  t.turn();
  t.port.time = 800;
  t.turn();
  t.port.time = 900;
  t.turn();
  const auto report = t.send("drv\n");
  CHECK(
      contains(report, "operation 21, address 255, function 0x06, register 0x0200, write word 5"));
  CHECK(contains(report, "elapsed 800 us, completed 900 us"));
  CHECK(contains(report, "Primary OK detail 0") && contains(report, "Cleanup TIMEOUT detail 0"));
  CHECK(t.owner.take(ticket, result) && result.status.code == Err::TIMEOUT);
  t.port.isolationVerified = true;
  CHECK(t.owner.recoverIsolated());
  CHECK(t.send("drv\n") == report);
  CHECK(t.port.writes.size() == 2 && t.owner.physicalAttempts() == 2);
}
}  // namespace

int main() {
  readDiagnostics();
  sharedHostBaud();
  vendorCleanup();
}
