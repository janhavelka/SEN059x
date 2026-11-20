#!/usr/bin/env python3
"""Own read-only SEN0591/SEN0592 Modbus RTU probe for USB–RS485 adapters.

Uses pyserial only for physical communication. Not a finished production driver.
This code intentionally does not offer configuration-writing CLI commands.
"""
import argparse
import math
import time

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
    """Read a one-register reply within the given number of seconds."""
    return _read_until(port, address, time.monotonic() + timeout_seconds)


def _read_until(port, address: int, deadline: float) -> int:
    frame_gap = 0.00175 if port.baudrate > 19200 else 3.5 * 10 / port.baudrate
    response = bytearray()
    required = 7
    last_byte_time = 0.0
    while True:
        # Only request available bytes so a caller's serial read timeout cannot extend ours.
        count = min(port.in_waiting, required + 1 - len(response))
        chunk = port.read(count) if count else b''
        now = time.monotonic()
        if now >= deadline:
            raise TimeoutError(f'Response deadline expired, received {response.hex(" ") or "no bytes"}')
        if chunk:
            response.extend(chunk)
            if len(response) >= 2:
                if response[1] not in (0x03, 0x83):
                    raise ValueError('Unexpected response header: ' + response.hex(' '))
                required = 5 if response[1] == 0x83 else 7
            if len(response) > required:
                raise ValueError('Overlong response: ' + response.hex(' '))
            last_byte_time = now
        elif len(response) == required and now - last_byte_time >= frame_gap:
            break
        time.sleep(min(0.001, deadline - now))

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
    request = make_read(address, register)
    if port.in_waiting:
        raise ValueError('Unexpected data on the bench bus before request')
    deadline = time.monotonic() + timeout_seconds
    if port.write(request) != len(request):
        raise OSError('Incomplete request write')
    # The auto-direction adapter handles TX turnaround while we wait for RX.
    return _read_until(port, address, deadline)


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
    parser.add_argument('--baud', type=int, default=115200,
                        choices=(4800, 9600, 14400, 19200, 38400, 57600, 76800, 115200))
    parser.add_argument('--address', type=int, default=1)
    parser.add_argument('--mode', choices=tuple(REGISTERS) + ('all',), default='realtime')
    parser.add_argument('--timeout', type=float, default=1.2, help='Whole measurement timeout, seconds')
    parser.add_argument('--model', choices=('SEN0591', 'SEN0592'))
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.port:
        parser.error('--port is required for a real measurement')
    if not args.model:
        parser.error('--model is required for a real measurement')
    if not 1 <= args.address <= 247:
        parser.error('Use a unicast address from 1 to 247')
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error('Use a finite positive timeout')
    try:
        import serial
    except ImportError:
        parser.error('Missing pyserial: pip install pyserial')
    modes = list(REGISTERS) if args.mode == 'all' else [args.mode]
    print(f'{args.model} address={args.address} baud={args.baud} on {args.port}')
    with serial.Serial(args.port, baudrate=args.baud, bytesize=8, parity='N', stopbits=1,
                       timeout=0, write_timeout=min(0.5, args.timeout)) as port:
        for mode in modes:
            reg, unit = REGISTERS[mode]
            started = time.monotonic()
            raw = read_register(port, args.address, reg, args.timeout)
            if mode == 'temperature':
                reading = f'{raw / 10:.1f} °C (unsigned scaling)'
            else:
                reading = f'{raw} {unit}'
            print(f'{mode}: {reading}, raw={raw}, elapsed={time.monotonic() - started:.3f}s')


if __name__ == '__main__':
    main()
