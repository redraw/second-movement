#!/usr/bin/env python3
"""Report linked flash and RAM bytes for faces in movement_config.h.

Usage: python3 utils/face_sizes.py --board sensorwatch_pro --display classic --sort flash-desc
The build is incremental. Pass --no-build to inspect an existing firmware.map.
"""

import argparse
from collections import defaultdict
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "movement_config.h"
MAP = ROOT / "build/firmware.map"
ELF = ROOT / "build/firmware.elf"
GROUPS = ("PRIMARY_FACES", "SECONDARY_FACES", "TERTIARY_FACES")
FACE_RE = re.compile(r"\bF\(([A-Za-z_][A-Za-z_0-9]*)\)")
SECTION_RE = re.compile(r"^([.][A-Za-z_][\w.]*)\s+0x[0-9a-fA-F]+\s+0x[0-9a-fA-F]+")
OBJECT_RE = re.compile(r"^\s+0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)\s+(?:\./)?build/([\w.-]+)\.o(?:\s|$)")


def configured_faces():
    groups = {name: [] for name in GROUPS}
    current = None
    for line in CONFIG.read_text().splitlines():
        macro = re.match(r"\s*#define\s+(\w+)\(F\)", line)
        if macro:
            current = macro.group(1) if macro.group(1) in groups else None
        if current:
            groups[current].extend(FACE_RE.findall(line))
            if not line.rstrip().endswith("\\"):
                current = None
    if not any(groups.values()):
        raise ValueError(f"No faces found in {CONFIG}")
    return groups


def linked_sizes():
    sizes = defaultdict(lambda: defaultdict(int))
    in_layout = False
    output_section = None
    for line in MAP.read_text().splitlines():
        if line == "Linker script and memory map":
            in_layout = True
            continue
        if not in_layout:
            continue  # The map lists discarded input sections before this point.
        section = SECTION_RE.match(line)
        if section:
            output_section = section.group(1)
            continue
        match = OBJECT_RE.match(line)
        if match and output_section in (".text", ".data", ".bss", ".flovl"):
            sizes[match.group(2)][output_section] += int(match.group(1), 16)
    return sizes


def firmware_totals():
    # Same GNU Arm size invocation used by gossamer's `make size` target.
    output = subprocess.run(
        ["arm-none-eabi-size", "-t", str(ELF)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    for line in output.splitlines():
        columns = line.split()
        if len(columns) >= 6 and columns[-1] == str(ELF):
            text, data, bss = map(int, columns[:3])
            return text + data, data + bss
    raise ValueError(f"Could not read firmware totals from arm-none-eabi-size:\n{output}")


def print_table(rows, total_flash, total_ram, firmware_flash, firmware_ram):
    headers = ("Face", "Group", "Flash (B)", "Flash %", "RAM (B)", "RAM %")
    body = [
        (face, group, f"{flash:,}", f"{flash / firmware_flash:.2%}",
         f"{ram:,}", f"{ram / firmware_ram:.2%}")
        for face, group, flash, ram in rows
    ]
    totals = [
        ("Configured faces", "", f"{total_flash:,}",
         f"{total_flash / firmware_flash:.2%}", f"{total_ram:,}",
         f"{total_ram / firmware_ram:.2%}"),
        ("Firmware total", "", f"{firmware_flash:,}", "100.00%",
         f"{firmware_ram:,}", "100.00%"),
    ]
    widths = [max(len(row[i]) for row in [headers, *body, *totals]) for i in range(len(headers))]

    def format_row(cells):
        return "  ".join(
            cell.ljust(widths[i]) if i < 2 else cell.rjust(widths[i])
            for i, cell in enumerate(cells)
        )

    rule = "  ".join("-" * width for width in widths)
    print(format_row(headers))
    print(rule)
    for row in body:
        print(format_row(row))
    print(rule)
    for row in totals:
        print(format_row(row))


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--board", default="sensorwatch_pro", help="Target board (default: sensorwatch_pro)")
    parser.add_argument("--display", default="classic", choices=("classic", "custom", "autodetect"), help="Display type (default: classic)")
    parser.add_argument("--no-build", action="store_true", help="Read the existing map without building")
    parser.add_argument(
        "--sort", default="flash-desc",
        choices=("flash-desc", "flash-asc", "ram-desc", "ram-asc", "name", "group"),
        help="Row order (default: flash-desc)",
    )
    args = parser.parse_args()

    if not args.no_build:
        command = ["make", f"BOARD={args.board}", f"DISPLAY={args.display}", "-j4"]
        if not MAP.exists() or (ELF.exists() and MAP.stat().st_mtime < ELF.stat().st_mtime):
            command.append("-B")  # An older build may have no map sidecar.
        build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        if build.returncode:
            sys.stderr.write(build.stdout)
            sys.stderr.write(build.stderr)
            return build.returncode
    if not MAP.exists():
        parser.error(f"Missing {MAP}; build the firmware to generate it")
    if not ELF.exists():
        parser.error(f"Missing {ELF}; build the firmware first")
    if MAP.stat().st_mtime < CONFIG.stat().st_mtime:
        parser.error("The map predates movement_config.h; rebuild before measuring")
    if MAP.stat().st_mtime < ELF.stat().st_mtime:
        parser.error("The map predates the ELF; rebuild before measuring")

    groups = configured_faces()
    sizes = linked_sizes()
    firmware_flash, firmware_ram = firmware_totals()
    rows = []
    total_flash = total_ram = 0
    missing = []
    for group, faces in groups.items():
        for face in faces:
            name = face + "_face"
            if name not in sizes:
                missing.append(face)
                continue
            parts = sizes[name]
            flash = parts[".text"] + parts[".data"] + parts[".flovl"]
            ram = parts[".data"] + parts[".bss"] + parts[".flovl"]
            total_flash += flash
            total_ram += ram
            rows.append((face, group.removesuffix("_FACES").lower(), flash, ram))
    sort_keys = {
        "flash-desc": lambda row: (-row[2], row[0]),
        "flash-asc": lambda row: (row[2], row[0]),
        "ram-desc": lambda row: (-row[3], row[0]),
        "ram-asc": lambda row: (row[3], row[0]),
        "name": lambda row: row[0],
        "group": lambda row: (GROUPS.index(row[1].upper() + "_FACES"), row[0]),
    }
    rows.sort(key=sort_keys[args.sort])

    print_table(rows, total_flash, total_ram, firmware_flash, firmware_ram)
    if missing:
        print(f"No linked sections for: {', '.join(missing)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
