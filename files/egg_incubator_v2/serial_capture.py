#!/usr/bin/env python3
"""
Simple serial capture for the incubator project.
Saves readable text (UTF-8, errors replaced) by default; use --raw to save raw bytes.
"""
import argparse
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except Exception as e:
    print("pyserial is required. Install with: python -m pip install pyserial")
    raise


def list_serial_ports():
    for p in list_ports.comports():
        print(f"{p.device}\t{p.description}")


def main():
    p = argparse.ArgumentParser(description='Capture serial to file')
    p.add_argument('--port', '-p', help='Serial port (e.g. COM3)', required=False)
    p.add_argument('--baud', '-b', type=int, default=115200, help='Baud rate')
    p.add_argument('--outfile', '-o', default='telemetry_log.txt', help='Output file')
    p.add_argument('--duration', '-d', type=int, default=120, help='Seconds to capture (0 = indefinite)')
    p.add_argument('--list', action='store_true', help='List available serial ports')
    p.add_argument('--raw', action='store_true', help='Write raw bytes instead of UTF-8 text')
    p.add_argument('--append', action='store_true', help='Append to outfile instead of overwrite')
    args = p.parse_args()

    if args.list:
        list_serial_ports()
        return

    if not args.port:
        print('Error: --port is required (use --list to discover ports)')
        sys.exit(2)

    mode = 'ab' if args.raw or args.append else 'wb' if args.raw else 'w'
    encoding = None if args.raw else 'utf-8'
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except Exception as e:
        print('Failed to open serial port:', e)
        sys.exit(1)

    print(f'Opened {args.port} @ {args.baud}, writing to {args.outfile} (raw={args.raw})')
    end = time.time() + args.duration if args.duration > 0 else None

    try:
        with open(args.outfile, mode, **({"encoding": encoding} if encoding else {})) as f:
            while True:
                if end is not None and time.time() > end:
                    break
                try:
                    data = ser.read(2048)
                except Exception as e:
                    print('Serial read error:', e)
                    break
                if not data:
                    continue
                if args.raw:
                    f.write(data)
                    f.flush()
                else:
                    try:
                        text = data.decode('utf-8', errors='replace')
                    except Exception:
                        text = str(data)
                    f.write(text)
                    f.flush()
    except KeyboardInterrupt:
        print('\nCapture interrupted by user')
    finally:
        try:
            ser.close()
        except Exception:
            pass

    print('Capture finished')


if __name__ == '__main__':
    main()
