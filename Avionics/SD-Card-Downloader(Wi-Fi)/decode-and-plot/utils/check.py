import struct
import sys


potential_magics = {
    0xDEADBEEF: 0,
    0x7F800000: 0,
    0xB33FD3AB: 0,
    0xB33FD3AD: 0,
    0xFF800000: 0,
}


def check_headers(file_path: str) -> None:
    if not file_path:
        assert file_path and len(file_path) != 0, "File path cannot be empty"

    count = 0
    window = b""

    with open(file_path, "rb") as f:
        window = f.read(4)
        while len(window) == 4:
            val = struct.unpack("<I", window)[0]

            if val in potential_magics:
                potential_magics[val] += 1

            new_byte = f.read(1)
            if not new_byte:
                break
            window = window[1:] + new_byte

    assert potential_magics != 0, "Could not find magic headers in the file"

    print("Headers found: ")
    for magic, count in potential_magics.items():
        print(f"{hex(magic).upper()}: {count}")

    assert (
        potential_magics[0xDEADBEEF] != 0
        or potential_magics[0x7F800000] != 0
        or potential_magics[0xB33FD3AB] != 0
        or potential_magics[0xB33FD3AD] != 0
        or potential_magics[0xFF800000] != 0
    ), "Could not find any headers necessary for the file. Please make sure the binary file is correct"
