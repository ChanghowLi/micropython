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


def read_pins(filename):
    """Read Pin objects, IRQs, and alternate functions from the AF table."""

    pins = []
    af_masks = {}
    irq_channels = {}
    irq_deep_standby = {}
    sci_spi_afs = []
    seen = set()

    with open(filename, newline="", encoding="utf-8-sig") as csv_file:
        reader = csv.DictReader(csv_file)
        required_fields = ["CPU_PIN", "IRQ"] + [
            f"AF{psel}" for psel in range(1, 32)
        ]

        if reader.fieldnames != required_fields:
            raise ValueError(
                f"{filename}: expected columns CPU_PIN,IRQ,AF1,...,AF31"
            )

        for row_number, row in enumerate(reader, start=2):
            cpu_name = row["CPU_PIN"].strip()

            port, bit = parse_pin_name(cpu_name)

            if cpu_name in seen:
                raise ValueError(
                    f"{filename}:{row_number}: duplicate pin: {cpu_name}"
                )

            seen.add(cpu_name)
            pins.append((cpu_name, cpu_name, port, bit))
            af_masks[cpu_name] = 0
            irq_channels[cpu_name] = -1
            irq_deep_standby[cpu_name] = False
            irq = row["IRQ"].strip()
            if irq:
                match = re.fullmatch(r"IRQ([0-9]|[12][0-9]|3[01])(-DS)?", irq)
                if match is None:
                    raise ValueError(
                        f"{filename}:{row_number}: invalid IRQ: {irq}"
                    )

                irq_channels[cpu_name] = int(match.group(1), 10)
                irq_deep_standby[cpu_name] = match.group(2) is not None

            for psel in range(1, 32):
                af = row[f"AF{psel}"].strip()
                if not af:
                    continue

                af_masks[cpu_name] |= 1 << psel

                if not af.startswith("SCI:"):
                    continue

                for signal, channel, group in re.findall(
                    r"(MISO|MOSI|SCK)([0-9]+)_([A-Z])", af
                ):
                    sci_spi_afs.append(
                        (
                            cpu_name,
                            port,
                            bit,
                            int(channel, 10),
                            group,
                            signal,
                        )
                    )

    return pins, af_masks, irq_channels, irq_deep_standby, sci_spi_afs


def write_header(filename):
    """Generate declarations shared by pin.c and generated pin source."""

    with open(filename, "w", encoding="utf-8", newline="\n") as output:
        output.write(
            "#ifndef MICROPY_INCLUDED_RENESAS_RA8_GENHDR_PINS_H\n"
            "#define MICROPY_INCLUDED_RENESAS_RA8_GENHDR_PINS_H\n"
            "\n"
            "#include <stddef.h>\n"
            "\n"
            "extern const struct _machine_pin_obj_t *const machine_pin_generated_pins[];\n"
            "extern const size_t machine_pin_generated_pins_count;\n"
            "\n"
            "#endif\n"
        )


def write_source(
    filename, pins, af_masks, irq_channels, irq_deep_standby, sci_spi_afs
):
    """Generate Pin objects, lookup arrays, and SCI SPI metadata."""

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
                f"    .irq_channel = {irq_channels[cpu_name]},\n"
                f"    .irq_deep_standby = "
                f"{'true' if irq_deep_standby[cpu_name] else 'false'},\n"
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
            "    MP_ARRAY_SIZE(machine_pin_generated_pins);\n\n"
            "const machine_pin_af_obj_t machine_pin_sci_spi_afs[] = {\n"
        )

        for cpu_name, port, bit, channel, group, signal in sci_spi_afs:
            output.write(
                "    {\n"
                f"        .pin = BSP_IO_PORT_{port:02d}_PIN_{bit:02d},\n"
                f"        .channel = {channel},\n"
                f"        .group = '{group}',\n"
                f"        .signal = MACHINE_PIN_AF_SCI_{signal},\n"
                "    },\n"
            )

        output.write(
            "};\n\n"
            "const size_t machine_pin_sci_spi_afs_count =\n"
            "    MP_ARRAY_SIZE(machine_pin_sci_spi_afs);\n"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--af-csv", required=True)
    parser.add_argument("--output-header", required=True)
    parser.add_argument("--output-source", required=True)
    args = parser.parse_args()

    pins, af_masks, irq_channels, irq_deep_standby, sci_spi_afs = read_pins(
        args.af_csv
    )
    write_header(args.output_header)
    write_source(
        args.output_source,
        pins,
        af_masks,
        irq_channels,
        irq_deep_standby,
        sci_spi_afs,
    )


if __name__ == "__main__":
    main()
