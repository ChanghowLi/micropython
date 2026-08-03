#!/usr/bin/env python3

import argparse
import csv
import re


def parse_pin_name(pin_name):
    """Convert a pin name such as P000 or PC09 into its port and bit."""

    match = re.fullmatch(r"P([0-9A-F])([0-9]{2})", pin_name)
    if match is None:
        raise ValueError(f"invalid pin name: {pin_name}")

    port = int(match.group(1), 16)
    bit = int(match.group(2), 10)

    if bit > 15:
        raise ValueError(f"invalid pin bit: {pin_name}")

    return port, bit


def read_board_pins(filename):
    """Read and validate safe board pins from pins.csv."""

    pins = []

    with open(filename, newline="", encoding="utf-8") as csv_file:
        for row_number, row in enumerate(csv.reader(csv_file), start=1):
            if not row:
                continue

            if len(row) != 2:
                raise ValueError(
                    f"{filename}:{row_number}: expected 2 columns"
                )

            board_name = row[0].strip()
            cpu_name = row[1].strip()
            port, bit = parse_pin_name(cpu_name)

            pins.append((board_name, cpu_name, port, bit))

    return pins


def read_af_masks(filename, pins):
    """Read alternate-function PSEL values and build a mask for each safe pin."""

    af_masks = {cpu_name: 0 for _board_name, cpu_name, _port, _bit in pins}
    seen = set()

    if filename is None:
        return af_masks

    with open(filename, newline="", encoding="utf-8-sig") as csv_file:
        reader = csv.DictReader(csv_file)
        required_fields = {"CPU_PIN", "PSEL", "PERIPHERAL", "SIGNAL", "GROUP"}

        if reader.fieldnames is None or set(reader.fieldnames) != required_fields:
            raise ValueError(
                f"{filename}: expected columns "
                "CPU_PIN,PSEL,PERIPHERAL,SIGNAL,GROUP"
            )

        for row_number, row in enumerate(reader, start=2):
            cpu_name = row["CPU_PIN"].strip()
            psel_text = row["PSEL"].strip()

            parse_pin_name(cpu_name)

            if cpu_name not in af_masks:
                raise ValueError(
                    f"{filename}:{row_number}: alternate function for "
                    f"non-safe pin: {cpu_name}"
                )

            try:
                psel = int(psel_text, 10)
            except ValueError as exc:
                raise ValueError(
                    f"{filename}:{row_number}: invalid PSEL: {psel_text}"
                ) from exc

            if not 1 <= psel <= 31:
                raise ValueError(
                    f"{filename}:{row_number}: PSEL must be from 1 to 31"
                )

            key = (cpu_name, psel)
            if key in seen:
                raise ValueError(
                    f"{filename}:{row_number}: duplicate PSEL {psel} "
                    f"for {cpu_name}"
                )

            seen.add(key)
            af_masks[cpu_name] |= 1 << psel

    return af_masks


def write_header(filename):
    """Generate declarations shared by pin.c and generated pin source."""

    with open(filename, "w", encoding="utf-8", newline="\n") as output:
        output.write(
            "#ifndef MICROPY_INCLUDED_RENESAS_RA8_GENHDR_PINS_H\n"
            "#define MICROPY_INCLUDED_RENESAS_RA8_GENHDR_PINS_H\n"
            "\n"
            "extern const machine_pin_obj_t *const machine_pin_generated_pins[];\n"
            "extern const size_t machine_pin_generated_pins_count;\n"
            "\n"
            "#endif\n"
        )


def write_source(filename, pins, af_masks):
    """Generate Pin objects and the safe-pin lookup array."""

    with open(filename, "w", encoding="utf-8", newline="\n") as output:
        output.write('#include "peripheral/pin.h"\n\n')

        for _board_name, cpu_name, port, bit in pins:
            output.write(
                f"static const machine_pin_obj_t "
                f"machine_pin_{cpu_name}_obj = {{\n"
                f"    .base = {{ &machine_pin_type }},\n"
                f"    .name = MP_QSTR_{cpu_name},\n"
                f"    .pin = BSP_IO_PORT_{port:02d}_PIN_{bit:02d},\n"
                f"    .alt_mask = 0x{af_masks[cpu_name]:08x}U,\n"
                f"}};\n\n"
            )

        output.write(
            "static const mp_rom_map_elem_t "
            "machine_pin_cpu_pins_locals_dict_table[] = {\n"
        )

        for _board_name, cpu_name, _port, _bit in pins:
            output.write(
                f"    {{ MP_ROM_QSTR(MP_QSTR_{cpu_name}), "
                f"MP_ROM_PTR(&machine_pin_{cpu_name}_obj) }},\n"
            )

        output.write(
            "};\n"
            "MP_DEFINE_CONST_DICT(machine_pin_cpu_pins_locals_dict, "
            "machine_pin_cpu_pins_locals_dict_table);\n\n"
            "static const mp_rom_map_elem_t "
            "machine_pin_board_pins_locals_dict_table[] = {\n"
        )

        for board_name, cpu_name, _port, _bit in pins:
            output.write(
                f"    {{ MP_ROM_QSTR(MP_QSTR_{board_name}), "
                f"MP_ROM_PTR(&machine_pin_{cpu_name}_obj) }},\n"
            )

        output.write(
            "};\n"
            "MP_DEFINE_CONST_DICT(machine_pin_board_pins_locals_dict, "
            "machine_pin_board_pins_locals_dict_table);\n\n"
        )

        output.write(
            "const machine_pin_obj_t *const "
            "machine_pin_generated_pins[] = {\n"
        )

        for _board_name, cpu_name, _port, _bit in pins:
            output.write(f"    &machine_pin_{cpu_name}_obj,\n")

        output.write(
            "};\n\n"
            "const size_t machine_pin_generated_pins_count =\n"
            "    MP_ARRAY_SIZE(machine_pin_generated_pins);\n"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--af-csv")
    parser.add_argument("--board-csv", required=True)
    parser.add_argument("--output-header", required=True)
    parser.add_argument("--output-source", required=True)
    args = parser.parse_args()

    pins = read_board_pins(args.board_csv)
    af_masks = read_af_masks(args.af_csv, pins)
    write_header(args.output_header)
    write_source(args.output_source, pins, af_masks)


if __name__ == "__main__":
    main()
