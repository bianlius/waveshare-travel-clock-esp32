"""Prevent application source drift between firmware targets and desktop builds."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_sources import shared_sources,source_groups

groups=source_groups()
assert set(groups)=={'app','network','renderer','viewport'}
application=set(groups['app'])
assert set((ROOT/'main/PAGE').rglob('*.c'))<=application
assert ROOT/'main/app_runtime.c' in application
assert ROOT/'ports/waveshare/main.c' not in application
assert set((ROOT/'main').rglob('*.c'))<=set(shared_sources('app','network'))
assert not any('ports' in p.relative_to(ROOT).parts for paths in groups.values() for p in paths)
all_files=[p for paths in groups.values() for p in paths]
assert len(all_files)==len(set(all_files))

cmake=shutil.which('cmake')
if cmake:
    with tempfile.TemporaryDirectory(prefix='travel-shared-sources-') as temp:
        folder=Path(temp)
        for selection in [('app','network'),('app',),('renderer','viewport'),('renderer',),('viewport',)]:
            destination=folder/'sources.txt'
            script=folder/'sources.cmake'
            script.write_text('cmake_minimum_required(VERSION 3.16)\n'
                f'include("{ROOT.as_posix()}/cmake/shared_sources.cmake")\n'
                f'travel_shared_sources(selected {" ".join(selection)})\n'
                'string(REPLACE ";" "\\n" lines "${selected}")\n'
                f'file(WRITE "{destination.as_posix()}" "${{lines}}")\n')
            subprocess.run([cmake,'-P',str(script)],check=True)
            assert destination.read_text().splitlines()==list(map(str,shared_sources(*selection))),selection
    print('PASS: CMake and desktop select identical shared sources for all target groups')
else:
    print('CMake source-list comparison: NOT RUN (CMake is available in the IDF full gate)')
print(f'PASS: {len(all_files)} unique shared sources, all pages and common runtime registered once')
