#!/usr/bin/env python3
import sys
import os


def generate_font_header(input_file, output_file, variable_name):
    with open(input_file, 'rb') as f:
        font_data = f.read()

    file_size = len(font_data)

    lines = []
    lines.append("#pragma once\n")
    lines.append("#include <cstddef>\n")
    lines.append("namespace EmbeddedFonts {\n")
    lines.append(f"static const unsigned char {variable_name}[] = {{")

    for i in range(0, len(font_data), 16):
        chunk = font_data[i:i+16]
        hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
        lines.append(f"    {hex_values},")

    lines.append("};")
    lines.append(f"\nstatic const size_t {variable_name}_size = {file_size};")
    lines.append("\n}")

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    print(f"Generated {output_file} ({file_size} bytes)")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: generate_font_header.py <input_font> <output_header> <variable_name>")
        sys.exit(1)

    input_font = sys.argv[1]
    output_header = sys.argv[2]
    variable_name = sys.argv[3]

    try:
        generate_font_header(input_font, output_header, variable_name)
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
