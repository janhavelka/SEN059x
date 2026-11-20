#!/usr/bin/env python3
"""Own read-only SEN0591/SEN0592 Modbus RTU probe for USB–RS485 adapters.

Uses pyserial only for physical communication. Not a finished production driver.
This code intentionally does not offer configuration-writing CLI commands.
"""
import argparse
import time
from typing import Optional

REGISTERS = {
    'processed': (0x0100, 'mm'),
    'realtime': (0x0101, 'mm'),
    'temperature': (0x0102, '0.1 °C'),
    'echo': (0x010A, 'us'),
}


def crc16_modbus(buf: bytes) -> int:
    value = 0xFFFF
    for byte in buf:
        value ^= byte
        for _ in range(8):
            if value & 1:
                value = (value >> 1) ^ 0xA001
            else:
                value >>= 1
    return value & 0xFFFF


def with_crc(payload: bytes) -> bytes:
    crc = crc16_modbus(payload)
    return payload + bytes((crc & 0xFF, crc >> 8))


def make_read(address: int, register: int) -> bytes:
    if not 1 <= address <= 247:
        raise ValueError('Use a unicast Modbus address 1..247; vendor documents broadcast ambiguously.')
    if not 0 <= register <= 0xFFFF:
        raise ValueError('Invalid register')
    return with_crc(bytes((address, 0x03, register >> 8, register & 0xFF, 0, 1)))


def read_response(port, address: int, timeout_seconds: float) -> int:
    deadline = time.monotonic() + timeout_seconds
    response = bytearray()
    required = 3
    while time.monotonic() < deadline:
        chunk = port.read(max(1, required - len(response)))
        if chunk:
            response.extend(chunk)
            if len(response) >= 2:
                if response[1] == 0x83:
                    required = 5
                elif response[1] == 0x03:
                    required = 7
                else:
                    raise ValueError('Unexpected function code ' + hex(response[1]))
            if len(response) >= required:
                break
    if len(response) != required:
        raise TimeoutError(f'Incomplete response {len(response)}/{required} bytes: {response.hex(" ")}')
    if crc16_modbus(response[:-2]) != (response[-2] | response[-1] << 8):
        raise ValueError('Bad CRC: ' + response.hex(' '))
    if response[0] != address:
        raise ValueError(f'Wrong address: {response[0]} expected {address}')
    if response[1] == 0x83:
        raise ValueError(f'Modbus exception code 0x{response[2]:02X}')
    if response[2] != 2:
        raise ValueError('Unexpected data length')
    return response[3] << 8 | response[4]


def read_register(port, address: int, register: int, timeout_seconds: float) -> int:
    port.reset_input_buffer()  # isolate stale bytes for single-threaded bench test
    request = make_read(address, register)
    port.write(request)
    port.flush()
    return read_response(port, address, timeout_seconds)


def self_test() -> None:
    expected = {
        0x0100: '01 03 01 00 00 01 85 F6',
        0x0101: '01 03 01 01 00 01 D4 36',
        0x0102: '01 03 01 02 00 01 24 36',
        0x0201: '01 03 02 01 00 01 D4 72',
    }
    for reg, frame in expected.items():
        assert make_read(1, reg) == bytes.fromhex(frame)
    assert with_crc(bytes.fromhex('01 06 02 00 00 05')) == bytes.fromhex('01 06 02 00 00 05 48 71')
    for hx in (
        '01 03 02 02 F2 38 A1', '01 03 02 02 EF F8 A8',
        '01 03 02 01 2C B8 09', '01 03 02 00 03 F8 45',
    ):
        bb = bytes.fromhex(hx)
        assert crc16_modbus(bb[:-2]) == (bb[-2] | bb[-1] << 8)
    print('CRC / command / official frame tests PASSED (no physical sensor required)')


def main():
    parser = argparse.ArgumentParser(description='Read-only DFRobot SEN059x Modbus RTU sensor probe')
    parser.add_argument('--self-test', action='store_true', help='Run offline CRC and frame tests')
    parser.add_argument('--port', help='USB-RS485 port, e.g. COM16 or /dev/ttyUSB0')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--address', type=int, default=1)
    parser.add_argument('--mode', choices=tuple(REGISTERS) + ('all',), default='realtime')
    parser.add_argument('--timeout', type=float, default=1.2, help='Whole measurement timeout, seconds')
    parser.add_argument('--model', choices=('SEN0591', 'SEN0592'), default='SEN0592')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.port:
        parser.error('--port is required for a real measurement')
    if args.timeout <= 0:
        parser.error('Positive timeout required')
    try:
        import serial
    except ImportError:
        parser.error('Missing pyserial: pip install pyserial')
    modes = list(REGISTERS) if args.mode == 'all' else [args.mode]
    print(f'{args.model} address={args.address} baud={args.baud} on {args.port}')
    with serial.Serial(args.port, baudrate=args.baud, bytesize=8, parity='N', stopbits=1,
                       timeout=0.05, write_timeout=0.5) as port:
        for mode in modes:
            reg, unit = REGISTERS[mode]
            started = time.monotonic()
            raw = read_register(port, args.address, reg, args.timeout)
            value = raw / 10 if mode == 'temperature' else raw
            print(f'{mode}: {value} {"°C" if mode == "temperature" else unit}, raw={raw}, elapsed={time.monotonic() - started:.3f}s')


if __name__ == '__main__':
    main()
