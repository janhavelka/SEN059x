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

  // Isolate leftover bytes; single owner / one outstanding query on the bus.
  while (bus.available() > 0) bus.read();
  digitalWrite(SEN059X_DE_GPIO, HIGH);
  delayMicroseconds(30);  // fixture-dependent transceiver-enable stabilization
  if (bus.write(command, sizeof(command)) != sizeof(command)) {
    digitalWrite(SEN059X_DE_GPIO, LOW);
    return false;
  }
  bus.flush();  // Arduino ESP32: wait until TX bytes have left UART
  digitalWrite(SEN059X_DE_GPIO, LOW);

  uint8_t response[7] = {};
  uint8_t received = 0;
  const uint32_t started = millis();
  while (static_cast<uint32_t>(millis() - started) < timeoutMs) {
    if (bus.available()) {
      uint8_t ch = static_cast<uint8_t>(bus.read());
      if (received < sizeof(response)) response[received++] = ch;
      // A normal response is 7B; 5B exception response is handled below.
      if (received >= 5 && response[1] == static_cast<uint8_t>(0x03 | 0x80))
        return false;
      if (received == sizeof(response)) break;
    } else {
      delay(1);
    }
  }
  if (received != sizeof(response) || response[0] != slave ||
      response[1] != 0x03 || response[2] != 0x02)
    return false;
  if (crc16(response, 5) != static_cast<uint16_t>(response[5] | (response[6] << 8)))
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
    Serial.println("Timeout, Modbus exception, or invalid response");
  }
  delay(500);
}
