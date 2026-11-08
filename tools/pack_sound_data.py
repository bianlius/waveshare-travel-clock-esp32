"""Pack the MP3 catalog into the raw sounddata partition image."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
PARTITION_SIZE = 12 * 1024 * 1024
ALIGN = 4096
COUNT = 10
HEADER = struct.Struct("<4sII" + "II" * COUNT)


def align(value):
    return (value + ALIGN - 1) // ALIGN * ALIGN


def pack():
    entries = []
    payload = bytearray(b"\xff" * ALIGN)
    for slot in range(COUNT):
        path = ROOT / "main" / "assets" / f"track_{slot:02d}.mp3"
        data = path.read_bytes()
        if len(data) < 4 or data[:1] != b"\xff" or data[1] & 0xE6 != 0xE2:
            raise ValueError(f"Invalid 12 kHz MP3: {path}")
        offset = align(len(payload))
        payload.extend(b"\xff" * (offset - len(payload)))
        entries.extend((offset, len(data)))
        payload.extend(data)
    if len(payload) > PARTITION_SIZE:
        raise ValueError(f"MP3 image is {len(payload)} bytes; partition is {PARTITION_SIZE}")
    payload[:HEADER.size] = HEADER.pack(b"GMP3", 1, COUNT, *entries)
    output = ROOT / "build" / "sounddata.bin"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    print(f"{output}: {len(payload)} bytes ({PARTITION_SIZE-len(payload)} bytes free)")


if __name__ == "__main__":
    pack()
