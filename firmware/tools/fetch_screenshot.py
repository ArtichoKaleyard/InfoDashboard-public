#!/usr/bin/env python3
"""Fetch the ESP32 dashboard screen snapshot.

The firmware serves the final RLCD framebuffer as a PBM P4 image. This script
always writes the PBM and optionally converts it to PNG when Pillow is present
or when the output path ends with .png.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from urllib.request import urlopen


def fetch(url: str, timeout: float) -> bytes:
    """Fetch raw screenshot bytes."""

    with urlopen(url, timeout=timeout) as response:
        return response.read()


def parse_pbm_header(payload: bytes) -> tuple[int, int, int]:
    """Return width, height, and payload offset for a binary PBM image."""

    if not payload.startswith(b"P4"):
        raise ValueError("screenshot is not PBM P4")
    tokens: list[bytes] = []
    index = 2
    while index < len(payload) and len(tokens) < 2:
        while index < len(payload) and payload[index] in b" \t\r\n":
            index += 1
        if index < len(payload) and payload[index] == ord("#"):
            while index < len(payload) and payload[index] not in b"\r\n":
                index += 1
            continue
        start = index
        while index < len(payload) and payload[index] not in b" \t\r\n":
            index += 1
        if start != index:
            tokens.append(payload[start:index])
    while index < len(payload) and payload[index] in b" \t\r\n":
        index += 1
    if len(tokens) != 2:
        raise ValueError("PBM header is incomplete")
    return int(tokens[0]), int(tokens[1]), index


def save_png_from_pbm(payload: bytes, output: Path) -> None:
    """Save PBM payload as PNG using Pillow."""

    from PIL import Image

    width, height, offset = parse_pbm_header(payload)
    row_bytes = (width + 7) // 8
    pixels = bytearray(width * height)
    body = payload[offset:]
    if len(body) < row_bytes * height:
        raise ValueError("PBM body is truncated")
    for y in range(height):
        row = body[y * row_bytes : (y + 1) * row_bytes]
        for x in range(width):
            black = (row[x >> 3] & (0x80 >> (x & 0x07))) != 0
            pixels[y * width + x] = 0 if black else 255
    Image.frombytes("L", (width, height), bytes(pixels)).save(output)


def main() -> int:
    """Run the screenshot fetcher."""

    parser = argparse.ArgumentParser(description="Fetch ESP32 dashboard screenshot")
    parser.add_argument("host", help="ESP32 host or URL")
    parser.add_argument("-o", "--output", default="screen.pbm", help="Output .pbm or .png path")
    parser.add_argument("--port", type=int, default=8080, help="Screenshot HTTP port")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout in seconds")
    args = parser.parse_args()

    if args.host.startswith("http://") or args.host.startswith("https://"):
        url = args.host
    else:
        url = f"http://{args.host}:{args.port}/screenshot.pbm"
    output = Path(args.output)
    payload = fetch(url, args.timeout)
    parse_pbm_header(payload)
    if output.suffix.lower() == ".png":
        save_png_from_pbm(payload, output)
    else:
        output.write_bytes(payload)
    print(f"saved {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
