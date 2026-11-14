"""Exercise saved SSID migration and the screen-only encoding boundary."""

from pathlib import Path
import subprocess
import tempfile


root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="gview-wifi-ssid-") as folder:
    binary = Path(folder) / "test_wifi_ssid"
    subprocess.run([
        "cc", "-std=c11", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra",
        "-Wno-unused-parameter", "-fsanitize=address,undefined",
        "-I", str(root / "components"),
        str(root / "tests/test_wifi_ssid.c"),
        str(root / "main/wifi_ssid.c"),
        str(root / "components/GVIEW/utf8_gb2312_switch.c"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
