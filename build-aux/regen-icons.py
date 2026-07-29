#!/usr/bin/env python3
# regen-icons.py:
# Render the master SVG (assets/meowmenu.svg) to all PNG sizes used by the
# Xfce hicolor icon theme. Uses rsvg-convert when available (preferred,
# lighter), falls back to inkscape. The generated PNGs are committed to
# the repo (the documented behavior) so end-user builds do not require either tool.

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

SIZES = [16, 22, 24, 32, 48, 64, 128, 256]


def find_renderer():
    # Returns (tool_path, kind) where kind is 'rsvg' or 'inkscape'.
    p = shutil.which("rsvg-convert")
    if p:
        return p, "rsvg"
    p = shutil.which("inkscape")
    if p:
        return p, "inkscape"
    return None, None


def render(tool: str, kind: str, svg: Path, png: Path, size: int) -> None:
    if kind == "rsvg":
        cmd = [tool, "-w", str(size), "-h", str(size),
               "-f", "png", "-o", str(png), str(svg)]
    else:  # inkscape
        cmd = [tool, "--export-type=png",
               f"--export-filename={png}",
               f"--export-width={size}", f"--export-height={size}",
               str(svg)]
    subprocess.run(cmd, check=True)


def main() -> int:
    ap = argparse.ArgumentParser(description="Render master SVG to PNG sizes.")
    ap.add_argument("--input", required=True, help="path to master SVG")
    ap.add_argument("--output-dir", required=True, help="directory for PNG outputs")
    args = ap.parse_args()

    svg = Path(args.input)
    out_dir = Path(args.output_dir)
    if not svg.is_file():
        print(f"regen-icons.py: input not found: {svg}", file=sys.stderr)
        return 2
    out_dir.mkdir(parents=True, exist_ok=True)

    tool, kind = find_renderer()
    if not tool:
        print(
            "regen-icons.py: neither rsvg-convert nor inkscape was found in PATH.\n"
            "  Install one of:\n"
            "    sudo apt install librsvg2-bin   # preferred\n"
            "    sudo apt install inkscape       # fallback",
            file=sys.stderr,
        )
        return 3

    for size in SIZES:
        png = out_dir / f"hi{size}-app-meowmenu.png"
        render(tool, kind, svg, png, size)
        print(f"  rendered {png} ({size}x{size}) via {kind}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
