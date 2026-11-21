"""Validate the integer decoder against the existing player and catalog audio."""
from pathlib import Path
import ctypes
import json
import math
import hashlib
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
PORT=ROOT/'ports/ai-passport'
sys.path.insert(0,str(PORT/'tools'))
from host_audio import build_decoder

vendor=PORT/'components/micro_mp3'
source=json.loads((vendor/'SOURCE.json').read_text())
assert source['commit']=='f70fc223e11646497cc321ea449d346afe85d906'
for name,sha in source['sha256'].items():
    assert hashlib.sha256((vendor/name).read_bytes()).hexdigest()==sha,name

def load(path):
    player=ctypes.CDLL(str(path))
    for name in ('mp3_player_start','mp3_player_start_once'):
        function=getattr(player,name)
        function.argtypes=[ctypes.POINTER(ctypes.c_uint8),ctypes.c_size_t]
        function.restype=ctypes.c_bool
    player.mp3_player_is_active.restype=ctypes.c_bool
    player.mp3_player_render.argtypes=[ctypes.c_uint8]
    player.mp3_player_render.restype=ctypes.c_int16
    return player

with tempfile.TemporaryDirectory(prefix='passport-fixed-audio-') as temporary:
    folder=Path(temporary)
    fixed=load(build_decoder(folder))
    reference=folder/'reference.so'
    subprocess.run(['cc','-std=c11','-O2','-shared','-fPIC','-I'+str(ROOT/'main'),str(ROOT/'ports/waveshare/mp3_player.c'),'-lm','-o',str(reference)],check=True)
    floating=load(reference)
    correlations=[]
    for slot in range(10):
        data=(ROOT/f'main/assets/track_{slot:02d}.mp3').read_bytes()
        buffer=(ctypes.c_uint8*len(data)).from_buffer_copy(data)
        samples=[]
        for player in (fixed,floating):
            assert player.mp3_player_start_once(buffer,len(data))
            pcm=[player.mp3_player_render(80) for _ in range(24000)]
            assert min(pcm)<-100 and max(pcm)>100
            samples.append(pcm);player.mp3_player_stop()
        a,b=samples
        correlation=sum(x*y for x,y in zip(a,b))/math.sqrt(sum(x*x for x in a)*sum(y*y for y in b))
        correlations.append(correlation)
        assert correlation>0.995,(slot,correlation)
    # Headerless catalog files have 240-byte MPEG-2.5 frames, 576 samples each.
    short=(ROOT/'main/assets/track_07.mp3').read_bytes()[:240*12]
    buffer=(ctypes.c_uint8*len(short)).from_buffer_copy(short)
    assert fixed.mp3_player_start_once(buffer,len(short))
    once=[fixed.mp3_player_render(100) for _ in range(12*576)]
    assert fixed.mp3_player_is_active()
    assert fixed.mp3_player_render(100)==0 and not fixed.mp3_player_is_active()
    assert fixed.mp3_player_start(buffer,len(short))
    loop=[fixed.mp3_player_render(100) for _ in range(2*len(once))]
    assert loop[:len(once)]==once and loop[len(once):]==once
    fixed.mp3_player_stop()
    for length in range(4,32):
        truncated=(ctypes.c_uint8*length).from_buffer_copy(short[:length])
        assert fixed.mp3_player_start(truncated,length)
        for _ in range(600):fixed.mp3_player_render(100)
        assert not fixed.mp3_player_is_active()
    garbage=(ctypes.c_uint8*8)(*b'bad data')
    assert not fixed.mp3_player_start(garbage,8)
    print(f'PASS: fixed-point decoder, all ten tracks (minimum PCM correlation {min(correlations):.6f}), exact looping/one-shot and bounded malformed input')
