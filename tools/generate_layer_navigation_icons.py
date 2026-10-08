#!/usr/bin/env python3
"""Generate the approved 35x35 layer-navigation bitmaps for firmware and Entropy."""
import argparse
from pathlib import Path

FIRMWARE_ROOT = Path(__file__).resolve().parents[1]

# Pixel master for the previous-layer icon. The next-layer icon is its mirror.
# Each row has 35 pixels: # is lit, . is transparent. Keep this artwork in
# sync with the approved 35x35 preview before changing its shape.
PREVIOUS_LAYER_PIXELS = (
    "...................................",
    "...................................",
    "...................................",
    "...................................",
    "................#.#................",
    "...............#####...............",
    "..............#######..............",
    ".............#########.............",
    ".........#..#####.#####............",
    "........########...#####...........",
    ".......########.....#####..........",
    "......########.......#####.........",
    ".....########.........#####........",
    "....########...........#####.......",
    "...########.............#####......",
    "..########...............#####.....",
    ".########.................#####....",
    "..######...................###.....",
    ".########.................#####....",
    "..########...............#####.....",
    "...########.............#####......",
    "....########...........#####.......",
    ".....########.........#####........",
    "......########.......#####.........",
    ".......########.....#####..........",
    "........########...#####...........",
    ".........#..#####.#####............",
    ".............#########.............",
    "..............#######..............",
    "...............#####...............",
    "................#.#................",
    "...................................",
    "...................................",
    "...................................",
    "...................................",
)


def pack(rows: tuple[str, ...]) -> bytes:
    assert len(rows) == 35 and all(len(row) == 35 for row in rows)
    pixels = ''.join(rows)
    assert set(pixels) <= {"#", "."}
    return bytes(
        sum(0x80 >> bit for bit in range(8)
            if offset + bit < len(pixels) and pixels[offset + bit] == "#")
        for offset in range(0, len(pixels), 8)
    )


def icon(next_layer: bool) -> bytes:
    rows = PREVIOUS_LAYER_PIXELS
    return pack(tuple(row[::-1] for row in rows) if next_layer else rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--entropy-assets", type=Path, required=True,
                        help="matching Entropy assets directory")
    assets = parser.parse_args().entropy_assets
    previous, following = icon(False), icon(True)
    assert len(previous) == len(following) == 154 and previous != following
    output = ["#pragma once", "#include <stdint.h>",
              "// Rhombus-chevron 35x35 layer-navigation pictograms, row-major, MSB first."]
    for name, bitmap in (("layer_prev_icon", previous), ("layer_next_icon", following)):
        output.append(f"static const uint8_t {name}[154] = {{")
        output.extend("    " + ", ".join(f"0x{byte:02X}" for byte in bitmap[start:start + 14]) + ","
                      for start in range(0, len(bitmap), 14))
        output.append("};")
    (FIRMWARE_ROOT / "keyboards/ergohaven/macropad/layer_navigation_icons.h").write_text("\n".join(output) + "\n")
    assets.mkdir(parents=True, exist_ok=True)
    (assets / "display-layer-prev.bin").write_bytes(previous)
    (assets / "display-layer-next.bin").write_bytes(following)


if __name__ == "__main__":
    main()
