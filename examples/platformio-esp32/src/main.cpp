// Original minimal bench example for DFRobot SEN0591/SEN0592.
// ESP32 Arduino / PlatformIO. Reassign pins to your exact PCB before powering.
// RS485 transceiver: single DE + /RE tied; HIGH transmits, LOW receives.
#include <Arduino.h>

#ifndef SEN059X_RX_GPIO
#error Define SEN059X_RX_GPIO for your PCB
#endif
#ifndef SEN059X_TX_GPIO
#error Define SEN059X_TX_GPIO for your PCB
#endif
#ifndef SEN059X_DE_GPIO
#error Define SEN059X_DE_GPIO for your RS485 transceiver
#endif

static HardwareSerial &bus = Serial2;
static constexpr uint8_t kSlave = 1;
static constexpr uint32_t kFrameGapUs = 1750;  // RTU inter-frame silence above 19200 baud

static uint16_t crc16(const uint8_t *data, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b)
      crc = static_cast<uint16_t>((crc >> 1) ^ ((crc & 1) ? 0xA001 : 0));
  }
  return crc;
}

static bool readWord(uint8_t slave, uint16_t reg, uint16_t &word, uint32_t timeoutMs) {
  const uint8_t payload[] = {
      slave, 0x03, static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg), 0, 1};
  uint8_t command[8];
  memcpy(command, payload, sizeof(payload));
  const uint16_t check = crc16(payload, sizeof(payload));
  command[6] = static_cast<uint8_t>(check);
  command[7] = static_cast<uint8_t>(check >> 8);

  const uint32_t started = millis();
  // The previous poll has settled during the 500 ms dwell. Drain only bytes
  // already queued, then defer this poll if the dedicated bus is still active.
  const int staleBytes = bus.available();
  for (int i = 0; i < staleBytes; ++i)
    bus.read();
  delayMicroseconds(kFrameGapUs);
  if (bus.available() > 0)
    return false;
  digitalWrite(SEN059X_DE_GPIO, HIGH);
  delayMicroseconds(30);  // fixture-dependent transceiver-enable stabilization
  const size_t written = bus.write(command, sizeof(command));
  bus.flush(true);  // TX only: wait for the last stop bit and preserve queued RX
  digitalWrite(SEN059X_DE_GPIO, LOW);
  if (written != sizeof(command))
    return false;

  uint8_t response[7];
  size_t received = 0;
  uint32_t lastByteUs = 0;
  while (true) {
    if (static_cast<uint32_t>(millis() - started) >= timeoutMs)
      return false;
    const int ch = bus.read();
    if (ch >= 0) {
      if (received == sizeof(response))
        return false;
      response[received++] = static_cast<uint8_t>(ch);
      lastByteUs = micros();
    } else {
      if (received && static_cast<uint32_t>(micros() - lastByteUs) >= kFrameGapUs)
        break;
      delay(1);
    }
  }
  if (received < 5 || response[0] != slave)
    return false;
  const size_t crcOffset = received - 2;
  const uint16_t receivedCrc = response[crcOffset] | (response[crcOffset + 1] << 8);
  if (crc16(response, crcOffset) != receivedCrc)
    return false;
  if (response[1] == 0x83 && received == 5) {
    Serial.printf("Modbus exception 0x%02X\n", static_cast<unsigned>(response[2]));
    return false;
  }
  if (received != sizeof(response) || response[1] != 0x03 || response[2] != 2)
    return false;
  word = static_cast<uint16_t>(response[3] << 8 | response[4]);
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(SEN059X_DE_GPIO, OUTPUT);
  digitalWrite(SEN059X_DE_GPIO, LOW);  // receive mode
  bus.begin(115200, SERIAL_8N1, SEN059X_RX_GPIO, SEN059X_TX_GPIO);
  delay(1200);  // DFRobot startup time <= 1000 ms
  Serial.println("SEN059x read-only Modbus RTU test; no config changes");
}

void loop() {
  uint16_t distance = 0;
  if (readWord(kSlave, 0x0101, distance, 1200)) {
    Serial.printf("Real-time distance: %u mm\n", static_cast<unsigned>(distance));
  } else {
    Serial.println("Read failed. Retrying after the polling interval.");
  }
  delay(500);
}
