#!/usr/bin/env python3
#
# %kadu copyright begin%
# Copyright 2026 Kadu Qt6 port
# %kadu copyright end%
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

"""Generate a QML-friendly Unicode emoji catalogue.

The source is Unicode's emoji-test.txt. It already provides fully-qualified
emoji sequences, English short names and the official group/subgroup split.
Generated catalogues deliberately keep the names in English; category labels
are represented by stable IDs and are to be translated by the QML picker.

The Kadu build invokes this script from its versioned local source data. It can
also be run manually when updating the catalogue, for example:

    python varia/emoji/generate-emoji-catalog.py --format cpp

or provide a downloaded source file to make an update reproducible offline:

    python varia/emoji/generate-emoji-catalog.py --source emoji-test.txt --format cpp
"""

from __future__ import annotations

import argparse
import datetime as datetime_module
import json
import pathlib
import re
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from typing import Iterable


DEFAULT_SOURCE_URL = pathlib.Path(__file__).with_name("emoji-test.txt")
DEFAULT_OUTPUT = pathlib.Path(__file__).with_name("emoji-catalog.json")
DEFAULT_CPP_OUTPUT = pathlib.Path(__file__).with_name("emoji-catalog-generated.cpp")
SCHEMA_VERSION = 1

GROUP_IDS = {
    "Smileys & Emotion": "smileys",
    "People & Body": "people",
    "Animals & Nature": "animals",
    "Food & Drink": "food",
    "Travel & Places": "travel",
    "Activities": "activities",
    "Objects": "objects",
    "Symbols": "symbols",
    "Flags": "flags",
}

SKIN_TONES = {
    0x1F3FB: "light",
    0x1F3FC: "mediumLight",
    0x1F3FD: "medium",
    0x1F3FE: "mediumDark",
    0x1F3FF: "dark",
}

GROUP_PATTERN = re.compile(r"^# group: (?P<name>.+)$")
SUBGROUP_PATTERN = re.compile(r"^# subgroup: (?P<name>.+)$")
VERSION_PATTERN = re.compile(r"^# Version: (?P<version>.+)$")
EMOJI_PATTERN = re.compile(
    r"^\s*(?P<codepoints>[0-9A-F ]+)\s*;\s*fully-qualified\s*#\s*"
    r"(?P<emoji>\S+)\s+E(?P<emoji_version>[0-9.]+)\s+(?P<name>.+)$"
)


@dataclass
class Emoji:
    sequence: tuple[int, ...]
    value: str
    name: str
    emoji_version: str
    group: str
    subgroup: str
    tones: dict[str, str] = field(default_factory=dict)

    @property
    def has_modifier(self) -> bool:
        return any(codepoint in SKIN_TONES for codepoint in self.sequence)

    @property
    def base_sequence(self) -> tuple[int, ...]:
        return tuple(codepoint for codepoint in self.sequence if codepoint not in SKIN_TONES)


def read_source(source: str | pathlib.Path) -> tuple[str, str]:
    """Return source text and a human-readable source identifier."""

    source_path = pathlib.Path(source)
    if source_path.is_file():
        return source_path.read_text(encoding="utf-8"), str(source_path.resolve())

    with urllib.request.urlopen(source, timeout=30) as response:
        return response.read().decode("utf-8"), source


def parse_emojis(source: Iterable[str]) -> tuple[str, list[Emoji]]:
    unicode_version = "unknown"
    group = ""
    subgroup = ""
    emojis: list[Emoji] = []

    for raw_line in source:
        line = raw_line.rstrip("\n")
        if version_match := VERSION_PATTERN.match(line):
            unicode_version = version_match.group("version")
            continue
        if group_match := GROUP_PATTERN.match(line):
            group = group_match.group("name")
            continue
        if subgroup_match := SUBGROUP_PATTERN.match(line):
            subgroup = subgroup_match.group("name")
            continue
        if not (emoji_match := EMOJI_PATTERN.match(line)):
            continue
        if group not in GROUP_IDS:
            # "Component" contains modifier building blocks, not selectable
            # emoji. Its contents are represented by tone variants below.
            continue

        emojis.append(
            Emoji(
                sequence=tuple(int(part, 16) for part in emoji_match.group("codepoints").split()),
                value=emoji_match.group("emoji"),
                name=emoji_match.group("name"),
                emoji_version=emoji_match.group("emoji_version"),
                group=group,
                subgroup=subgroup,
            )
        )

    return unicode_version, emojis


def apply_skin_tone_variants(emojis: list[Emoji]) -> list[Emoji]:
    """Attach safe, single-tone variants and remove them from the main grid.

    Multi-person sequences can carry multiple independent modifiers. They stay
    available through their default fully-qualified emoji for now; presenting a
    single tone choice for such a sequence would be misleading.
    """

    bases = {emoji.sequence: emoji for emoji in emojis if not emoji.has_modifier}
    primary: list[Emoji] = []

    for emoji in emojis:
        modifiers = [codepoint for codepoint in emoji.sequence if codepoint in SKIN_TONES]
        if not modifiers:
            primary.append(emoji)
            continue
        if len(modifiers) != 1:
            continue

        base = bases.get(emoji.base_sequence)
        if base is not None:
            base.tones[SKIN_TONES[modifiers[0]]] = emoji.value

    return primary


def catalogue_data(unicode_version: str, source_id: str, emojis: list[Emoji]) -> dict[str, object]:
    categories: list[dict[str, object]] = []
    for group_name, category_id in GROUP_IDS.items():
        subgroups = sorted({emoji.subgroup for emoji in emojis if emoji.group == group_name})
        categories.append(
            {
                "id": category_id,
                "unicodeGroup": group_name,
                "subgroups": subgroups,
            }
        )

    return {
        "schemaVersion": SCHEMA_VERSION,
        "unicodeVersion": unicode_version,
        "source": source_id,
        "generatedAt": datetime_module.datetime.now(datetime_module.timezone.utc)
        .replace(microsecond=0)
        .isoformat(),
        "toneIds": ["light", "mediumLight", "medium", "mediumDark", "dark"],
        "categories": categories,
        "emojis": [
            {
                "emoji": emoji.value,
                "name": emoji.name,
                "emojiVersion": emoji.emoji_version,
                "category": GROUP_IDS[emoji.group],
                "subgroup": emoji.subgroup,
                "tones": emoji.tones,
            }
            for emoji in emojis
        ],
    }


def cpp_string(value: str) -> str:
    """Return a portable UTF-16 C++ string literal for a Unicode value."""

    escaped = []
    for codepoint in map(ord, value):
        if codepoint <= 0xFFFF:
            escaped.append(f"\\u{codepoint:04X}")
        else:
            escaped.append(f"\\U{codepoint:08X}")
    return 'u"' + "".join(escaped) + '"'


def generate_cpp_catalogue(unicode_version: str, emojis: list[Emoji], output: pathlib.Path, header: pathlib.Path) -> None:
    """Generate the static Qt catalogue used at runtime without JSON parsing."""

    entries: list[str] = []
    for emoji in emojis:
        tones = ", ".join(
            f"EmojiToneVariant{{{cpp_string(tone_id)}, {cpp_string(emoji.tones.get(tone_id, ''))}}}"
            for tone_id in ["light", "mediumLight", "medium", "mediumDark", "dark"]
        )
        entries.append(
            "EmojiCatalogueEntry{"
            f"{cpp_string(emoji.value)}, {cpp_string(emoji.name)}, {cpp_string(GROUP_IDS[emoji.group])}, "
            f"std::array<EmojiToneVariant, 5>{{{tones}}}" "}"
        )

    categories = ", ".join(cpp_string(category_id) for category_id in GROUP_IDS.values())
    header.write_text(
        """/* Generated by generate-emoji-catalog.py. Do not edit. */
#pragma once

#include <array>
#include <QStringView>
#include <span>

struct EmojiToneVariant
{
    QStringView id;
    QStringView value;
};

struct EmojiCatalogueEntry
{
    QStringView value;
    QStringView name;
    QStringView category;
    std::array<EmojiToneVariant, 5> tones;
};

std::span<const EmojiCatalogueEntry> emojiCatalogueEntries();
std::span<const QStringView> emojiCatalogueCategories();
QStringView emojiCatalogueUnicodeVersion();
""",
        encoding="utf-8",
    )
    output.write_text(
        """/* Generated by generate-emoji-catalog.py. Do not edit. */
#include \"emoji-catalog-generated.h\"

static constexpr auto generatedEmojiEntries = std::to_array<EmojiCatalogueEntry>({
"""
        + ",\n".join(f"    {entry}" for entry in entries)
        + "\n});\n\n"
        + "static constexpr auto generatedEmojiCategories = std::to_array<QStringView>({"
        + categories
        + "});\n\n"
        + "std::span<const EmojiCatalogueEntry> emojiCatalogueEntries()\n{\n"
        + "    return generatedEmojiEntries;\n}\n\n"
        + "std::span<const QStringView> emojiCatalogueCategories()\n{\n"
        + "    return generatedEmojiCategories;\n}\n\n"
        + "QStringView emojiCatalogueUnicodeVersion()\n{\n"
        + f"    return {cpp_string(unicode_version)};\n}}\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        default=DEFAULT_SOURCE_URL,
        help="URL or local emoji-test.txt file (default: versioned source next to this script)",
    )
    parser.add_argument("--format", choices=("json", "cpp"), default="json", help="generated catalogue format")
    parser.add_argument("--output", type=pathlib.Path, help="destination catalogue file")
    parser.add_argument("--header", type=pathlib.Path, help="generated C++ header (only with --format cpp)")
    arguments = parser.parse_args()

    try:
        source_text, source_id = read_source(arguments.source)
        unicode_version, parsed_emojis = parse_emojis(source_text.splitlines())
    except (OSError, UnicodeError, urllib.error.URLError) as error:
        print(f"Could not read Unicode emoji data: {error}", file=sys.stderr)
        return 1

    emojis = apply_skin_tone_variants(parsed_emojis)
    if not emojis:
        print("Unicode emoji data did not contain selectable emoji.", file=sys.stderr)
        return 1

    output = arguments.output or (DEFAULT_CPP_OUTPUT if arguments.format == "cpp" else DEFAULT_OUTPUT)
    output.parent.mkdir(parents=True, exist_ok=True)
    if arguments.format == "cpp":
        header = arguments.header or output.with_suffix(".h")
        header.parent.mkdir(parents=True, exist_ok=True)
        generate_cpp_catalogue(unicode_version, emojis, output, header)
        print(f"Generated {output} and {header} with {len(emojis)} emoji (Unicode {unicode_version}).")
    else:
        output.write_text(
            json.dumps(catalogue_data(unicode_version, source_id, emojis), ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        print(f"Generated {output} with {len(emojis)} emoji (Unicode {unicode_version}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
