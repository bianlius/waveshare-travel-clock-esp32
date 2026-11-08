"""Download the ten game rips and encode MP3 files for the 12 kHz codec."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from urllib.parse import quote
from urllib.request import Request, urlopen
import hashlib
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "build" / "mp3_sources"
OUTPUT_DIR = ROOT / "main" / "assets"
BASE = "https://fi.zophar.net/soundfiles/"
TRACKS = (
    ("gameboy-gbs/pokemon-red", "04 Pallet Town's Theme.mp3", "track_00.mp3"),
    ("gameboy-gbs/pokemon-red", "05 Pokemon Center.mp3", "track_01.mp3"),
    ("gameboy-gbs/pokemon-red", "12 Lavender Town's Theme.mp3", "track_02.mp3"),
    ("gameboy-gbs/pokemon-red", "23 Battle (VS Trainer).mp3", "track_03.mp3"),
    ("gameboy-gbs/pokemon-gold", "60 New Bark Town's Theme.mp3", "track_04.mp3"),
    ("gameboy-gbs/pokemon-gold", "35 National Park.mp3", "track_05.mp3"),
    ("gameboy-gbs/pokemon-gold", "44 Ecruteak City's Theme.mp3", "track_06.mp3"),
    ("gameboy-advance-gsf/pokemon-sapphire", "105 Littleroot Town.mp3", "track_07.mp3"),
    ("gameboy-advance-gsf/pokemon-sapphire", "142 Route 113.mp3", "track_08.mp3"),
    ("gameboy-advance-gsf/pokemon-sapphire", "132 Slateport City.mp3", "track_09.mp3"),
)
SOURCE_SHA256 = (
    "66fe326bdd19d0c994e3d49b754b0346ba5b0bab8b1aff635f55efda4b9655a6",
    "d0c262d0683c17cba431a95d07aed04962c4b4b19662c3272c601c10ac2e2f9e",
    "35c81c9baa0a1ffc1b3e62424521f2c6b12212bd99b475d06f37cc0905496894",
    "2b10a77f9ac619978b7042f17838448086adc29d2123367a19b73771e93ff273",
    "bb102ed32d59e0f2ca4ac87af0522b9fc9890f849ea7bf47192c1458b9dafd25",
    "ffa7f6f368dddf3db9964f3e046d4398f09a3571f0b8ab39eb665075097f454a",
    "5052d2c3c1acbfd4962531c4d56b69438829a3fb34d1376231cdd641027355b3",
    "26ae644e91bb1a702ddc5e4250282f92f717d871d1270f30ad85c73ea37da25d",
    "8607fec944824b3a6fc62c5c840e2a68fec23c25d122b8ed7228b4107b80d6d8",
    "a3194401821d88b30f3ea379dfef1a4c27dec43edf6c3e5286da3cd946e903bd",
)


def ffmpeg_executable():
    installed = shutil.which("ffmpeg")
    if installed:
        return installed
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()


def prepare(item):
    directory, filename, output_name = item
    source = SOURCE_DIR / output_name
    output = OUTPUT_DIR / output_name
    if not source.exists():
        url = BASE + directory + "/" + quote(filename)
        request = Request(url, headers={"User-Agent": "GVIEW-Engine firmware asset builder"})
        data = urlopen(request, timeout=90).read()
        if len(data) < 10000:
            raise RuntimeError(f"Download is too small: {url}")
        source.write_bytes(data)
    source_digest = hashlib.sha256(source.read_bytes()).hexdigest()
    expected = SOURCE_SHA256[int(output_name[6:8])]
    if source_digest != expected:
        raise RuntimeError(f"Source MP3 changed: {output_name}: {source_digest}")
    if not output.exists():
        subprocess.run([
            ffmpeg_executable(), "-hide_banner", "-loglevel", "error", "-y",
            "-i", str(source), "-vn", "-ar", "12000", "-ac", "1",
            "-codec:a", "libmp3lame", "-b:a", "40k", "-map_metadata", "-1",
            "-write_xing", "0", "-id3v2_version", "0", str(output),
        ], check=True)
    data = output.read_bytes()
    if len(data) < 4 or data[0] != 0xFF or data[1] & 0xE6 != 0xE2:
        raise RuntimeError(f"Invalid MPEG 2.5 Layer III header: {output}")
    return output_name, len(data), source_digest, hashlib.sha256(data).hexdigest()


def main():
    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=4) as pool:
        for name, size, source_hash, output_hash in pool.map(prepare, TRACKS):
            print(f"{name}: {size} bytes source={source_hash} output={output_hash}")


if __name__ == "__main__":
    main()
