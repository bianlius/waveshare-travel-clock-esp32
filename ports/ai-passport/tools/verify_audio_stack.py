#!/usr/bin/env python3
"""Bound the decoder call chain using the exact C3 compiler's stack report."""
from pathlib import Path
import re
import sys
import subprocess

build=Path(sys.argv[1])
port=Path(__file__).resolve().parents[1]
header=(port/'include/passport_audio.h').read_text()
budget=int(re.search(r'#define PASSPORT_AUDIO_STACK_BYTES (\d+)',header).group(1))
decoder={};worker=None
for path in build.rglob('*.su'):
    for line in path.read_text().splitlines():
        fields=line.split('\t')
        if len(fields)!=3:continue
        name=fields[0].rsplit(':',1)[-1]
        if 'mp3_player' in path.name or 'micro_mp3' in str(path):
            assert fields[2]=='static',f'Unbounded MP3 frame: {line}'
            decoder[str(path)+':'+name]=int(fields[1])
        if name=='soundTask':
            assert fields[2]=='static',f'Unbounded audio worker: {line}'
            worker=int(fields[1])
assert any('pvmp3_framedecoder' in name for name in decoder)
assert any('mp3_player_render' in name for name in decoder) and worker is not None
# Summing all decoder functions overestimates mutually exclusive paths. Keep
# another 4 KB for the worker's framework/libc calls and scheduler ABI margin.
required=sum(decoder.values())+worker+4096
assert required<=budget,f'Audio task stack insufficient: {required} > {budget}'
library=build/'esp-idf/micro_mp3/libmicro_mp3.a'
assert library.is_file(),'Missing fixed-point decoder library'
undefined=subprocess.check_output(['riscv32-esp-elf-nm','-u',str(library)],text=True)
assert not re.search(r'__(?:add|sub|mul|div)(?:sf|df)3|__float\w*(?:sf|df)',undefined), 'Decoder uses software floating-point arithmetic'
source=(port/'main/passport_main.c').read_text()
assert '"travel_audio",PASSPORT_AUDIO_STACK_BYTES,' in source
print(f'PASS: C3 fixed-point audio stack budget {budget} bytes >= conservative bound {required} bytes')
print('PASS: fixed-point decoder has no software floating-point arithmetic dependencies')
