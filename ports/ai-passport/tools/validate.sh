#!/usr/bin/env bash
set -euo pipefail
port_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
project_root="$(cd -- "${port_root}/../.." && pwd)"
mode="${1:---all}"

static_checks() {
    local host_python="${PYTHON_FOR_HOST:-python3}"
    env -u PYTHONNOUSERSITE -u PYTHONHOME -u PYTHONPATH -u __PYVENV_LAUNCHER__ "${host_python}" "${port_root}/tests/test_firmware_layout.py"
    env -u PYTHONNOUSERSITE -u PYTHONHOME -u PYTHONPATH -u __PYVENV_LAUNCHER__ "${host_python}" - "${project_root}" <<'PY'
import subprocess,sys
from pathlib import Path
root=Path(sys.argv[1]);logs=root/'artifacts/ai-passport/validation';logs.mkdir(parents=True,exist_ok=True)
tests=sorted((root/'tests').glob('test_*.py'))
for test in tests:
    log=logs/(test.stem+'.log')
    with log.open('wb') as output:
        result=subprocess.run([sys.executable,str(test)],cwd=root,stdout=output,stderr=subprocess.STDOUT,timeout=240)
    if result.returncode:
        print(log.read_text(errors='replace')[-6000:]);raise SystemExit(f'FAIL: {test.name}')
    print(f'PASS: {test.name}',flush=True)
print(f'Host tests: PASS ({len(tests)} Python checks)')
PY
    local host_dir
    host_dir="$(mktemp -d /tmp/travel-passport-host.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror "${project_root}/tests/test_shop_model.c" "${project_root}/main/PAGE/SHOP/shop_model.c" -o "${host_dir}/shop"
    "${host_dir}/shop"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror "${project_root}/tests/test_wifi_keyboard.c" "${project_root}/main/PAGE/INPUTTXT/wifi_keyboard.c" -o "${host_dir}/keyboard"
    "${host_dir}/keyboard"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"${project_root}/components/GVIEW" -I"${project_root}/main/pic_data/pic_default" "${project_root}/tests/test_scale_1bit.c" -o "${host_dir}/scale"
    "${host_dir}/scale"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"${port_root}/include" "${port_root}/tests/test_audio_policy.c" -o "${host_dir}/audio-policy"
    "${host_dir}/audio-policy"
    rm -rf -- "${host_dir}"
}

firmware_checks() (
    if ! command -v idf.py >/dev/null || [[ "$(idf.py --version)" != "ESP-IDF v5.5.3" ]]; then
        echo 'Activate ESP-IDF v5.5.3 before building AI Passport.' >&2;exit 1
    fi
    local validation_build
    validation_build="$(mktemp -d /tmp/travel-passport-firmware.XXXXXX)"
    trap 'case "${validation_build}" in /tmp/travel-passport-firmware.*) rm -rf -- "${validation_build}" ;; esac' EXIT
    cd "${port_root}"
    SDKCONFIG_DEFAULTS="${port_root}/sdkconfig.defaults" idf.py -B "${validation_build}" -D "SDKCONFIG=${validation_build}/sdkconfig" build
    idf.py -B "${validation_build}" merge-bin -o "${validation_build}/FoloToy-AI-Passport-full.bin"
    python tools/verify_firmware.py "${validation_build}"
    python tools/verify_audio_stack.py "${validation_build}"
    python tests/test_packed_audio.py "${validation_build}/sounddata.bin"
    mkdir -p build
    python tools/archive_firmware.py create "${validation_build}" --archive-root "${port_root}/build/firmware"
    # Retain only the current verified segmented files, including the audio image.
    python - "${validation_build}" "${port_root}/build" <<'PY'
from pathlib import Path
import shutil,sys
source,destination=map(Path,sys.argv[1:])
names=['FoloToy-AI-Passport-full.bin','FoloToy-AI-Passport.bin','FoloToy-AI-Passport.elf','FoloToy-AI-Passport.map','bootloader/bootloader.bin','partition_table/partition-table.bin','sounddata.bin','sounddata.json','flash_args']
for name in names:
    target=destination/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/name,target)
PY
    echo 'Build: PASS (verified ESP32-C3 image, protected identity, matching ELF/archive)'
)

case "${mode}" in
    --all) static_checks;firmware_checks ;;
    --static) static_checks ;;
    --firmware) firmware_checks ;;
    *) echo "Usage: $0 [--all|--static|--firmware]" >&2;exit 2 ;;
esac
