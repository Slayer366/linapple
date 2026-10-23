#!/usr/bin/env python3
import sys
from pathlib import Path

source, output = map(Path, sys.argv[1:3])
data = source.read_bytes()
with output.open("w") as header:
    header.write("static const unsigned char osk_png[] = {\n")
    for offset in range(0, len(data), 12):
        header.write("  " + ", ".join(f"0x{byte:02x}" for byte in data[offset:offset + 12]) + ",\n")
    header.write("};\n")
