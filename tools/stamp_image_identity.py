#!/usr/bin/env python3
"""Stamp and verify the fixed MSPM firmware image-identity block.

The implementation intentionally uses only the Python standard library so a
clean checkout needs no package installation before it can build firmware.
"""

import argparse
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence, Tuple


ELF_MAGIC = b"\x7fELF"
ELFCLASS32 = 1
ELFDATA2LSB = 1
PT_LOAD = 1
SHT_SYMTAB = 2

ELF32_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
ELF32_PROGRAM_HEADER = struct.Struct("<IIIIIIII")
ELF32_SECTION_HEADER = struct.Struct("<IIIIIIIIII")
ELF32_SYMBOL = struct.Struct("<IIIBBH")

IDENTITY_MAGIC = b"MSPM"
IDENTITY_FORMAT_VERSION = 1
IDENTITY_SIZE = 64
IDENTITY_CRC_OFFSET = 16
IDENTITY_CRC_SIZE = 4
IDENTITY_FLAG_SOURCE_DIRTY = 1 << 0
IDENTITY_FLAG_DEBUG = 1 << 1
IDENTITY_FLAGS_ALLOWED = IDENTITY_FLAG_SOURCE_DIRTY | IDENTITY_FLAG_DEBUG
IDENTITY_HEADER = struct.Struct("<4sHHBBHII16sI24s")


@dataclass(frozen=True)
class ProgramHeader:
    p_type: int
    p_offset: int
    p_paddr: int
    p_filesz: int


@dataclass(frozen=True)
class Section:
    name: str
    section_type: int
    address: int
    offset: int
    size: int
    link: int
    entry_size: int


@dataclass(frozen=True)
class ImageLayout:
    flash_origin: int
    flash_length: int
    data_load_end: int
    identity_address: int
    identity_offset: int
    identity_size: int
    coverage: bytes

    @property
    def identity_end(self) -> int:
        return self.identity_address + self.identity_size


def fail(message: str) -> None:
    raise ValueError(message)


def checked_slice(data: bytes, offset: int, size: int, label: str) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        fail(f"{label} is outside the ELF file")
    return data[offset : offset + size]


def c_string(table: bytes, offset: int, label: str) -> str:
    if offset < 0 or offset >= len(table):
        fail(f"{label} has an invalid string-table offset")
    end = table.find(b"\0", offset)
    if end < 0:
        fail(f"{label} is not NUL-terminated")
    try:
        return table[offset:end].decode("ascii")
    except UnicodeDecodeError as error:
        fail(f"{label} is not ASCII: {error}")


class Elf32Image:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = bytearray(path.read_bytes())
        if len(self.data) < ELF32_HEADER.size:
            fail(f"{path}: file is too small to be ELF32")

        header = ELF32_HEADER.unpack_from(self.data)
        ident = header[0]
        if ident[0:4] != ELF_MAGIC or ident[4] != ELFCLASS32 or ident[5] != ELFDATA2LSB:
            fail(f"{path}: expected a little-endian ELF32 image")

        self.program_header_offset = header[5]
        self.section_header_offset = header[6]
        self.program_header_size = header[9]
        self.program_header_count = header[10]
        self.section_header_size = header[11]
        self.section_header_count = header[12]
        self.section_name_index = header[13]
        if self.program_header_size != ELF32_PROGRAM_HEADER.size:
            fail(f"{path}: unexpected ELF32 program-header size")
        if self.section_header_size != ELF32_SECTION_HEADER.size:
            fail(f"{path}: unexpected ELF32 section-header size")

        self._raw_sections = self._read_sections()
        self.sections = self._name_sections()
        self.program_headers = self._read_program_headers()

    def _read_sections(self) -> List[Tuple[int, int, int, int, int, int, int, int]]:
        raw_sections: List[Tuple[int, int, int, int, int, int, int, int]] = []
        total_size = self.section_header_count * self.section_header_size
        checked_slice(self.data, self.section_header_offset, total_size, "section-header table")
        for index in range(self.section_header_count):
            offset = self.section_header_offset + index * self.section_header_size
            fields = ELF32_SECTION_HEADER.unpack_from(self.data, offset)
            raw_sections.append(
                (fields[0], fields[1], fields[3], fields[4], fields[5], fields[6], fields[8], fields[9])
            )
        return raw_sections

    def _name_sections(self) -> List[Section]:
        if self.section_name_index >= len(self._raw_sections):
            fail(f"{self.path}: invalid section-name string-table index")
        name_section = self._raw_sections[self.section_name_index]
        name_table = checked_slice(self.data, name_section[3], name_section[4], "section-name string table")
        sections: List[Section] = []
        for index, raw in enumerate(self._raw_sections):
            name_offset, section_type, address, offset, size, link, _alignment, entry_size = raw
            name = c_string(name_table, name_offset, f"section {index}") if index else ""
            sections.append(Section(name, section_type, address, offset, size, link, entry_size))
        return sections

    def _read_program_headers(self) -> List[ProgramHeader]:
        headers: List[ProgramHeader] = []
        total_size = self.program_header_count * self.program_header_size
        checked_slice(self.data, self.program_header_offset, total_size, "program-header table")
        for index in range(self.program_header_count):
            offset = self.program_header_offset + index * self.program_header_size
            fields = ELF32_PROGRAM_HEADER.unpack_from(self.data, offset)
            headers.append(ProgramHeader(fields[0], fields[1], fields[3], fields[4]))
        return headers

    def symbol_values(self) -> Dict[str, int]:
        values: Dict[str, int] = {}
        for section in self.sections:
            if section.section_type != SHT_SYMTAB:
                continue
            if section.entry_size != ELF32_SYMBOL.size or section.size % section.entry_size != 0:
                fail(f"{self.path}: malformed symbol table {section.name}")
            if section.link >= len(self.sections):
                fail(f"{self.path}: symbol table {section.name} has invalid string table")
            string_section = self.sections[section.link]
            strings = checked_slice(
                self.data, string_section.offset, string_section.size, f"symbol strings for {section.name}"
            )
            table = checked_slice(self.data, section.offset, section.size, f"symbol table {section.name}")
            for offset in range(0, len(table), section.entry_size):
                fields = ELF32_SYMBOL.unpack_from(table, offset)
                if fields[0] == 0:
                    continue
                name = c_string(strings, fields[0], f"symbol in {section.name}")
                if name:
                    # Local symbols (including Arm's repeated $t mapping
                    # symbols) are allowed to share a name.  The linker
                    # symbols consumed below are unique by construction.
                    values.setdefault(name, fields[1])
        return values

    def section_named(self, name: str) -> Section:
        matches = [section for section in self.sections if section.name == name]
        if len(matches) != 1:
            fail(f"{self.path}: expected exactly one {name} section")
        return matches[0]

    def write_identity(self, layout: ImageLayout, identity: bytes) -> None:
        if len(identity) != layout.identity_size:
            fail("internal error: identity size does not match the linker reservation")
        self.data[layout.identity_offset : layout.identity_offset + layout.identity_size] = identity

    def save(self) -> None:
        self.path.write_bytes(self.data)


def parse_bcd_version(value: str) -> Tuple[int, int]:
    parts = value.split(".")
    if len(parts) != 2 or any(len(part) != 2 or not part.isdigit() for part in parts):
        fail(f"version must be two BCD byte pairs, got {value!r}")
    major = int(parts[0][0]) * 16 + int(parts[0][1])
    minor = int(parts[1][0]) * 16 + int(parts[1][1])
    return major, minor


def parse_flag(value: str) -> int:
    if value not in ("0", "1"):
        raise argparse.ArgumentTypeError("must be 0 or 1")
    return int(value)


def make_layout(image: Elf32Image, flash_origin: int, flash_length: int) -> ImageLayout:
    if flash_length <= IDENTITY_SIZE:
        fail("flash length is too small for the image-identity reservation")
    symbols = image.symbol_values()
    if "__data_load_end" not in symbols:
        fail(f"{image.path}: missing __data_load_end linker symbol")
    data_load_end = symbols["__data_load_end"]
    identity = image.section_named(".image_identity")
    expected_identity_address = flash_origin + flash_length - IDENTITY_SIZE
    if identity.address != expected_identity_address or identity.size != IDENTITY_SIZE:
        fail(
            f"{image.path}: .image_identity must be {IDENTITY_SIZE} bytes at "
            f"0x{expected_identity_address:08x}"
        )
    if data_load_end <= flash_origin or data_load_end > identity.address:
        fail(f"{image.path}: __data_load_end is outside the defined-content range")
    checked_slice(image.data, identity.offset, identity.size, ".image_identity")

    coverage_length = data_load_end - flash_origin
    coverage = bytearray(coverage_length)
    covered = bytearray(coverage_length)
    for header in image.program_headers:
        if header.p_type != PT_LOAD or header.p_filesz == 0:
            continue
        segment_start = header.p_paddr
        segment_end = header.p_paddr + header.p_filesz
        overlap_start = max(segment_start, flash_origin)
        overlap_end = min(segment_end, data_load_end)
        if overlap_start >= overlap_end:
            continue
        coverage_start = overlap_start - flash_origin
        coverage_end = overlap_end - flash_origin
        if any(covered[coverage_start:coverage_end]):
            fail(f"{image.path}: overlapping loadable segments in CRC coverage")
        file_start = header.p_offset + (overlap_start - segment_start)
        coverage[coverage_start:coverage_end] = checked_slice(
            image.data, file_start, overlap_end - overlap_start, "loadable segment"
        )
        covered[coverage_start:coverage_end] = b"\x01" * (coverage_end - coverage_start)

    try:
        first_missing = covered.index(0)
    except ValueError:
        first_missing = -1
    if first_missing >= 0:
        fail(
            f"{image.path}: no loadable byte at 0x{flash_origin + first_missing:08x} "
            "inside CRC coverage"
        )

    return ImageLayout(
        flash_origin,
        flash_length,
        data_load_end,
        identity.address,
        identity.offset,
        identity.size,
        bytes(coverage),
    )


def encode_source_id(source_id: str) -> bytes:
    try:
        encoded = source_id.encode("ascii")
    except UnicodeEncodeError as error:
        fail(f"source identifier is not ASCII: {error}")
    if not encoded or len(encoded) > 16 or any(byte < 0x21 or byte > 0x7E for byte in encoded):
        fail("source identifier must contain 1 to 16 printable ASCII characters")
    return encoded.ljust(16, b"\0")


def build_identity(
    version_major: int,
    version_minor: int,
    source_id: bytes,
    flags: int,
    image_span: int,
    content_length: int,
    crc32: int,
) -> bytes:
    return IDENTITY_HEADER.pack(
        IDENTITY_MAGIC,
        IDENTITY_FORMAT_VERSION,
        IDENTITY_SIZE,
        version_major,
        version_minor,
        flags,
        image_span,
        crc32,
        source_id,
        content_length,
        b"\xff" * 24,
    )


def crc32_iso_hdlc(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def decode_identity(identity: bytes, layout: ImageLayout) -> Tuple[int, int, int, int, bytes, int, int]:
    if len(identity) != IDENTITY_SIZE:
        fail("image identity has an invalid size")
    (
        magic,
        format_version,
        header_size,
        major,
        minor,
        flags,
        span,
        crc32,
        source_id,
        content_length,
        reserved,
    ) = IDENTITY_HEADER.unpack(identity)
    if magic != IDENTITY_MAGIC:
        fail("image identity magic is invalid")
    if format_version != IDENTITY_FORMAT_VERSION or header_size != IDENTITY_SIZE:
        fail("image identity format or header size is unsupported")
    if flags & ~IDENTITY_FLAGS_ALLOWED:
        fail("image identity contains unsupported flags")
    if span != layout.identity_end - layout.flash_origin:
        fail("image identity flash span does not match the linker reservation")
    if content_length != len(layout.coverage):
        fail("image identity content length does not match __data_load_end")
    if reserved != b"\xff" * len(reserved):
        fail("image identity reserved bytes are not erased-value 0xff")
    source_end = source_id.find(b"\0")
    source_text = source_id if source_end < 0 else source_id[:source_end]
    if not source_text or any(byte < 0x21 or byte > 0x7E for byte in source_text):
        fail("image identity source identifier is invalid")
    if source_end >= 0 and source_id[source_end:] != b"\0" * (len(source_id) - source_end):
        fail("image identity source identifier is not NUL-padded")

    crc_input_identity = bytearray(identity)
    crc_input_identity[IDENTITY_CRC_OFFSET : IDENTITY_CRC_OFFSET + IDENTITY_CRC_SIZE] = b"\0" * IDENTITY_CRC_SIZE
    expected_crc32 = crc32_iso_hdlc(layout.coverage + bytes(crc_input_identity))
    if crc32 != expected_crc32:
        fail(f"image identity CRC32 is 0x{crc32:08x}; expected 0x{expected_crc32:08x}")
    return major, minor, flags, crc32, source_text, span, content_length


def verify_elf(image: Elf32Image, layout: ImageLayout) -> bytes:
    identity = bytes(image.data[layout.identity_offset : layout.identity_offset + layout.identity_size])
    major, minor, flags, crc32, source_id, span, content_length = decode_identity(identity, layout)
    print(
        f"{image.path}: image identity OK: version={major:02x}.{minor:02x} "
        f"source={source_id.decode('ascii')} flags=0x{flags:04x} span=0x{span:x} "
        f"content=0x{content_length:x} crc32=0x{crc32:08x}"
    )
    return identity


def verify_binary(path: Path, layout: ImageLayout, identity: bytes) -> None:
    data = path.read_bytes()
    if len(data) != layout.identity_end - layout.flash_origin:
        fail(f"{path}: expected a {layout.identity_end - layout.flash_origin}-byte flash image")
    if data[: len(layout.coverage)] != layout.coverage:
        fail(f"{path}: defined content before __data_load_end differs from the ELF")
    gap_start = layout.data_load_end - layout.flash_origin
    gap_end = layout.identity_address - layout.flash_origin
    if data[gap_start:gap_end] != b"\xff" * (gap_end - gap_start):
        fail(f"{path}: erased gap is not serialized as 0xff")
    identity_start = layout.identity_address - layout.flash_origin
    if data[identity_start:] != identity:
        fail(f"{path}: image identity differs from the ELF")
    print(f"{path}: matches canonical ELF")


def parse_intel_hex(path: Path) -> Dict[int, int]:
    values: Dict[int, int] = {}
    base = 0
    saw_eof = False
    for line_number, line in enumerate(path.read_text(encoding="ascii").splitlines(), start=1):
        if not line.startswith(":"):
            fail(f"{path}:{line_number}: record does not start with ':'")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as error:
            fail(f"{path}:{line_number}: invalid hexadecimal record: {error}")
        if len(record) < 5 or len(record) != record[0] + 5:
            fail(f"{path}:{line_number}: record length is invalid")
        if sum(record) & 0xFF:
            fail(f"{path}:{line_number}: record checksum is invalid")
        count = record[0]
        address = (record[1] << 8) | record[2]
        record_type = record[3]
        payload = record[4 : 4 + count]
        if saw_eof:
            fail(f"{path}:{line_number}: data follows the EOF record")
        if record_type == 0:
            absolute_address = base + address
            for offset, value in enumerate(payload):
                byte_address = absolute_address + offset
                if byte_address in values:
                    fail(f"{path}:{line_number}: duplicate byte at 0x{byte_address:08x}")
                values[byte_address] = value
        elif record_type == 1:
            if count != 0 or address != 0:
                fail(f"{path}:{line_number}: malformed EOF record")
            saw_eof = True
        elif record_type == 2:
            if count != 2 or address != 0:
                fail(f"{path}:{line_number}: malformed extended-segment record")
            base = ((payload[0] << 8) | payload[1]) << 4
        elif record_type == 4:
            if count != 2 or address != 0:
                fail(f"{path}:{line_number}: malformed extended-linear record")
            base = ((payload[0] << 8) | payload[1]) << 16
        elif record_type in (3, 5):
            if count != 4 or address != 0:
                fail(f"{path}:{line_number}: malformed start-address record")
        else:
            fail(f"{path}:{line_number}: unsupported record type {record_type}")
    if not saw_eof:
        fail(f"{path}: no EOF record")
    return values


def verify_hex(path: Path, layout: ImageLayout, identity: bytes) -> None:
    expected: Dict[int, int] = {
        layout.flash_origin + offset: value for offset, value in enumerate(layout.coverage)
    }
    expected.update({layout.identity_address + offset: value for offset, value in enumerate(identity)})
    actual = parse_intel_hex(path)
    if actual != expected:
        missing = sorted(set(expected) - set(actual))
        unexpected = sorted(set(actual) - set(expected))
        if missing:
            fail(f"{path}: missing byte at 0x{missing[0]:08x}")
        if unexpected:
            fail(f"{path}: contains unexpected byte at 0x{unexpected[0]:08x}")
        differing = next(address for address in expected if expected[address] != actual[address])
        fail(f"{path}: differs from the ELF at 0x{differing:08x}")
    print(f"{path}: matches canonical ELF")


def stamp(args: argparse.Namespace) -> int:
    image = Elf32Image(args.elf)
    layout = make_layout(image, args.flash_origin, args.flash_length)
    major, minor = parse_bcd_version(args.version)
    flags = (IDENTITY_FLAG_SOURCE_DIRTY if args.source_dirty else 0) | (
        IDENTITY_FLAG_DEBUG if args.debug else 0
    )
    source_id = encode_source_id(args.source_id)
    identity_without_crc = build_identity(
        major,
        minor,
        source_id,
        flags,
        layout.identity_end - layout.flash_origin,
        len(layout.coverage),
        0,
    )
    crc32 = crc32_iso_hdlc(layout.coverage + identity_without_crc)
    identity = build_identity(
        major,
        minor,
        source_id,
        flags,
        layout.identity_end - layout.flash_origin,
        len(layout.coverage),
        crc32,
    )
    image.write_identity(layout, identity)
    image.save()
    verify_elf(image, layout)
    return 0


def verify(args: argparse.Namespace) -> int:
    image = Elf32Image(args.elf)
    layout = make_layout(image, args.flash_origin, args.flash_length)
    identity = verify_elf(image, layout)
    if args.bin is not None:
        verify_binary(args.bin, layout, identity)
    if args.hex is not None:
        verify_hex(args.hex, layout, identity)
    return 0


def verify_readback(args: argparse.Namespace) -> int:
    image = Elf32Image(args.elf)
    layout = make_layout(image, args.flash_origin, args.flash_length)
    identity = verify_elf(image, layout)
    readback = args.readback.read_bytes()
    if readback != identity:
        fail(f"{args.readback}: target read-back does not match the canonical ELF identity")
    print(f"{args.readback}: target read-back matches canonical ELF")
    return 0


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subcommands = parser.add_subparsers(dest="command", required=True)

    def add_layout_arguments(command: argparse.ArgumentParser) -> None:
        command.add_argument("--flash-origin", type=lambda value: int(value, 0), required=True)
        command.add_argument("--flash-length", type=lambda value: int(value, 0), required=True)

    stamp_parser = subcommands.add_parser("stamp", help="patch the canonical ELF identity")
    stamp_parser.add_argument("--elf", type=Path, required=True)
    stamp_parser.add_argument("--version", required=True)
    stamp_parser.add_argument("--source-id", required=True)
    stamp_parser.add_argument("--source-dirty", type=parse_flag, required=True)
    stamp_parser.add_argument("--debug", action="store_true")
    add_layout_arguments(stamp_parser)
    stamp_parser.set_defaults(handler=stamp)

    verify_parser = subcommands.add_parser("verify", help="verify the ELF and optional BIN/HEX artifacts")
    verify_parser.add_argument("--elf", type=Path, required=True)
    verify_parser.add_argument("--bin", type=Path)
    verify_parser.add_argument("--hex", type=Path)
    add_layout_arguments(verify_parser)
    verify_parser.set_defaults(handler=verify)

    readback_parser = subcommands.add_parser("verify-readback", help="compare a target read-back with an ELF")
    readback_parser.add_argument("--elf", type=Path, required=True)
    readback_parser.add_argument("--readback", type=Path, required=True)
    add_layout_arguments(readback_parser)
    readback_parser.set_defaults(handler=verify_readback)
    return parser


def main(argv: Sequence[str]) -> int:
    parser = make_parser()
    args = parser.parse_args(argv)
    try:
        return args.handler(args)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"image identity error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
