#!/usr/bin/env python3
"""Generate FM-1 coverage masks from the Android monospace TTF.

Usage: python3 tools/generate_display_font.py /path/to/DroidSansMono.ttf
Requires Pillow. The generated local include is not a source font.
"""
import argparse
import hashlib
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SIZES = (9, 16, 22, 28)
FIRST, LAST = 32, 126
# Keep below the available application-image margin.
MAX_BYTES = 40000

def masks(path):
    offsets, runs = [0], []
    for scale, size in enumerate(SIZES, 1):
        width, height = 6 * scale, 8 * scale
        font = ImageFont.truetype(str(path), size)
        # Center the common baseline range rather than each individual
        # character; ascenders and descenders remain in their proper positions.
        boxes = [font.getbbox(chr(ch), anchor="ls") for ch in range(FIRST, LAST + 1)]
        top, bottom = min(b[1] for b in boxes), max(b[3] for b in boxes)
        render_height = max(height, bottom - top)
        baseline = (render_height - (bottom - top)) // 2 - top
        for ch in range(FIRST, LAST + 1):
            char = chr(ch)
            canvas = Image.new("L", (width, render_height))
            advance = font.getlength(char)
            x = (width - advance) / 2
            ImageDraw.Draw(canvas).text((x, baseline), char, font=font,
                                        anchor="ls", fill=255)
            if render_height != height:
                canvas = canvas.resize((width, height), Image.Resampling.LANCZOS)
            values = [(v * 15 + 127) // 255 for v in canvas.getdata()]
            i = 0
            while i < len(values):
                n = 1
                while i + n < len(values) and n < 16 and values[i+n] == values[i]:
                    n += 1
                runs.append((values[i] << 4) | (n - 1))
                i += n
            offsets.append(len(runs))
    if len(runs) + 2 * len(offsets) > MAX_BYTES:
        raise SystemExit("Font exceeds display asset budget")
    return offsets, runs

def array(name, ctype, values, per_line):
    lines = [f"static const {ctype} {name}[] = {{"]
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(str(v) for v in values[i:i+per_line]) + ",")
    return "\n".join(lines + ["};", ""])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path)
    parser.add_argument("--output", type=Path,
                        default=ROOT / "firmware/app/display_font_data.inc")
    args = parser.parse_args()
    offsets, runs = masks(args.font)
    digest = hashlib.sha256(args.font.read_bytes()).hexdigest()
    args.output.write_text(
        f"/* Android monospace coverage masks; source SHA-256: {digest} */\n"
        + array("fm1_font_offsets", "uint16_t", offsets, 12)
        + array("fm1_font_runs", "uint8_t", runs, 20),
        encoding="ascii",
    )
    print(f"{args.output}: {len(runs) + 2 * len(offsets)} bytes of font data")
    print(f"Source SHA-256: {digest}")

if __name__ == "__main__":
    main()
