"""Compile the exact Passport fixed-point backend for native audio checks."""
from pathlib import Path
import subprocess

PORT = Path(__file__).resolve().parents[1]
ROOT = PORT.parents[1]
DECODER = PORT / "components/micro_mp3"


def build_decoder(directory):
    library = Path(directory) / "passport-mp3.so"
    sources = [PORT / "main/mp3_player.cpp", DECODER / "src/mp3_decoder.cpp",
               *sorted((DECODER / "src/opencore-mp3dec").glob("*.cpp"))]
    subprocess.run(["c++", "-std=c++14", "-O2", "-shared", "-fPIC",
                    "-I" + str(ROOT / "main"), "-I" + str(DECODER / "include"),
                    "-I" + str(DECODER / "src/opencore-mp3dec"),
                    *map(str, sources), "-o", str(library)], check=True)
    return library
