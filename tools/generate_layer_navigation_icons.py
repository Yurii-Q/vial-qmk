#!/usr/bin/env python3
"""Generate matching two-tier 35x35 layer icons for firmware and Entropy."""
from pathlib import Path
from PIL import Image, ImageDraw

FIRMWARE_ROOT = Path(__file__).resolve().parents[1]
ENTROPY_ASSETS = Path(__file__).resolve().parents[2] / "entropy-autoswitch-ui-kde/assets"


def icon(next_layer: bool) -> bytes:
    image = Image.new("1", (35, 35), 0)
    draw = ImageDraw.Draw(image)
    # One large layer and one lower contour. The old third contour made the
    # 35-pixel artwork unnecessarily dense.
    draw.line([(17, 3), (32, 12), (17, 21), (2, 12), (17, 3)], fill=1, width=3, joint="curve")
    draw.line([(2, 20), (17, 30), (32, 20)], fill=1, width=3, joint="curve")
    if next_layer:
        draw.line([(11, 12), (24, 12)], fill=1, width=3)
        draw.line([(20, 8), (24, 12), (20, 16)], fill=1, width=3, joint="curve")
    else:
        draw.line([(11, 12), (24, 12)], fill=1, width=3)
        draw.line([(15, 8), (11, 12), (15, 16)], fill=1, width=3, joint="curve")
    pixels = list(image.getdata())
    return bytes(sum(0x80 >> bit for bit in range(8) if offset + bit < len(pixels) and pixels[offset + bit])
                 for offset in range(0, len(pixels), 8))


def main() -> None:
    previous, following = icon(False), icon(True)
    assert len(previous) == len(following) == 154 and previous != following
    output = ["#pragma once", "#include <stdint.h>",
              "// Two-tier 35x35 layer-navigation pictograms, row-major, MSB first."]
    for name, data in (("layer_prev_icon", previous), ("layer_next_icon", following)):
        output.append(f"static const uint8_t {name}[154] = {{")
        output.extend("    " + ", ".join(f"0x{byte:02X}" for byte in data[start:start + 14]) + ","
                      for start in range(0, len(data), 14))
        output.append("};")
    (FIRMWARE_ROOT / "keyboards/ergohaven/macropad/layer_navigation_icons.h").write_text("\n".join(output) + "\n")
    ENTROPY_ASSETS.mkdir(exist_ok=True)
    (ENTROPY_ASSETS / "display-layer-prev.bin").write_bytes(previous)
    (ENTROPY_ASSETS / "display-layer-next.bin").write_bytes(following)


if __name__ == "__main__":
    main()
