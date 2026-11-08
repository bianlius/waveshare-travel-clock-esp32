"""Verify that every MP3 in the raw Flash image matches its catalog entry."""

from pathlib import Path
import struct
import subprocess
import sys


root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(root / "tools/pack_sound_data.py")], check=True)
image = (root / "build/sounddata.bin").read_bytes()
header = struct.Struct("<4sII" + "II" * 10)
fields = header.unpack_from(image)
assert fields[:3] == (b"GMP3", 1, 10)
last_end = 4096
for slot in range(10):
    offset, length = fields[3 + slot * 2:5 + slot * 2]
    asset = (root / f"main/assets/track_{slot:02d}.mp3").read_bytes()
    assert offset % 4096 == 0 and offset >= last_end
    assert length == len(asset) and image[offset:offset + length] == asset
    last_end = offset + length
assert last_end == len(image) and len(image) <= 12 * 1024 * 1024
print("PASS: ten MP3 tracks, aligned offsets, and partition capacity")
