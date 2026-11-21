#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Regenerate the port's new title lettering from the pinned Reggae One font.

Requires Pillow only for regeneration. Does not read any game files.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

FONT_SHA256 = "aebe62598732d76036f30ec11bb0ec5f68938e373a06d1b4ceb6b9cf1abf3db2"
FONT_REVISION = "fe9be09d02ad8ba5e71d5ce24efc5249fe77dcfc"
PROMPTS = (
    ("english", "Press any button!", 152),
    ("french", "Appuyez sur une touche !", 174),
    ("spanish", "¡Pulsa cualquier botón!", 145),
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path, help="ReggaeOne-Regular.ttf from the pinned Fontworks repository")
    parser.add_argument("output", type=Path, help="Path to the C++ header")
    arguments = parser.parse_args()

    from PIL import Image, ImageDraw, ImageFont

    if hashlib.sha256(arguments.font.read_bytes()).hexdigest() != FONT_SHA256:
        parser.error(f"expected fontworks-fonts/Reggae at {FONT_REVISION}")
    font = ImageFont.truetype(str(arguments.font), 96)
    lines = [
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "// Rasterized port text from Reggae One by Fontworks Inc. (SIL OFL 1.1).",
        f"// https://github.com/fontworks-fonts/Reggae/tree/{FONT_REVISION}",
        "// Regenerate with tools/testing/render_title_prompt.py. No game data is used.",
        "#pragma once",
        "#include <array>",
        "#include <string_view>",
        "",
        "namespace st::title_art {",
    ]
    for identifier, message, limit in PROMPTS:
        x, y, right, bottom = font.getbbox(message)
        image = Image.new("L", (right - x, bottom - y))
        ImageDraw.Draw(image).text((-x, -y), message, 255, font=font)
        image = image.crop(image.getbbox())
        height = 15
        width = min(limit, round(image.width * height / image.height))
        image = image.resize((width, height), Image.Resampling.LANCZOS)
        lines.append(f"inline constexpr std::array<std::string_view, {height}> {identifier}{{{{")
        for row in range(height):
            ink = "".join(
                "@" if coverage >= 178 else "+" if coverage >= 100 else "." if coverage >= 56 else " "
                for coverage in (image.getpixel((column, row)) for column in range(width))
            )
            lines.append(f'    "{ink}",')
        lines.extend(("}};", ""))
    lines.extend(("} // namespace st::title_art", ""))
    arguments.output.write_text("\n".join(lines), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
