"""Decode an MP3 catalog track in looping and one-shot modes."""

import ctypes
import hashlib
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
asset = (ROOT / "main/assets/track_07.mp3").read_bytes()
assert hashlib.sha256(asset).hexdigest() == (
    "ce65db42e148ac04cc629055cbd31b18fe6ad30c678acfeca3b2add0cf110912"
)

with tempfile.TemporaryDirectory() as directory:
    library = Path(directory) / "libmp3.dylib"
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
         str(ROOT / "main/mp3_player.c"), "-lm", "-o", str(library)],
        check=True,
    )
    player = ctypes.CDLL(str(library))
    player.mp3_player_start.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    player.mp3_player_start.restype = ctypes.c_bool
    player.mp3_player_start_once.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    player.mp3_player_start_once.restype = ctypes.c_bool
    player.mp3_player_is_active.restype = ctypes.c_bool
    player.mp3_player_render.argtypes = [ctypes.c_uint8]
    player.mp3_player_render.restype = ctypes.c_int16

    garbage = (ctypes.c_uint8 * 8)(*b"bad data")
    assert not player.mp3_player_start(garbage, len(garbage))
    assert not player.mp3_player_is_active()

    buffer = (ctypes.c_uint8 * len(asset)).from_buffer_copy(asset)
    assert player.mp3_player_start(buffer, len(asset))
    first = [player.mp3_player_render(80) for _ in range(24_000)]
    assert min(first) < -500 and max(first) > 500
    # The downloaded recording is about 117 seconds; playback must wrap cleanly.
    for _ in range(120 * 12_000):
        player.mp3_player_render(80)
    after_loop = [player.mp3_player_render(80) for _ in range(12_000)]
    assert any(after_loop) and player.mp3_player_is_active()
    player.mp3_player_stop()
    assert not player.mp3_player_is_active()

    for slot in range(10):
        track = (ROOT / f"main/assets/track_{slot:02d}.mp3").read_bytes()
        track_buffer = (ctypes.c_uint8 * len(track)).from_buffer_copy(track)
        assert player.mp3_player_start_once(track_buffer, len(track))
        samples = [player.mp3_player_render(80) for _ in range(24_000)]
        assert any(samples), f"track {slot} decoded as silence"
        player.mp3_player_stop()

    assert player.mp3_player_start_once(buffer, len(asset))
    for _ in range(120 * 12_000):
        player.mp3_player_render(80)
    assert not player.mp3_player_is_active()

print("PASS: ten MP3 tracks, audible PCM, looping, one-shot end, and malformed input")
