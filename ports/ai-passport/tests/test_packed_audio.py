"""Decode the actual reduced-size catalog with the device's MP3 implementation."""
from pathlib import Path
import ctypes
import struct
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from host_audio import build_decoder

ROOT=Path(__file__).resolve().parents[3]
data=Path(sys.argv[1]).read_bytes()
assert struct.unpack_from('<4sII',data)==(b'GMP3',1,10)
assert len(data)<=0x4a6000
with tempfile.TemporaryDirectory(prefix='passport-audio-') as temporary:
    library=build_decoder(temporary)
    decoder=ctypes.CDLL(str(library))
    decoder.mp3_player_start_once.argtypes=[ctypes.POINTER(ctypes.c_uint8),ctypes.c_size_t]
    decoder.mp3_player_start_once.restype=ctypes.c_bool
    decoder.mp3_player_render.argtypes=[ctypes.c_uint8]
    decoder.mp3_player_render.restype=ctypes.c_int16
    for slot in range(10):
        offset,length=struct.unpack_from('<II',data,12+slot*8)
        packed=data[offset:offset+length]
        assert len(packed)==length and offset%4096==0
        # All source frames are 40 kbit/s MPEG-2.5/12kHz (240 bytes);
        # the output uses 16 kbit/s (96 bytes). Preserve full track duration.
        source=(ROOT/'main/assets'/f'track_{slot:02d}.mp3').read_bytes()
        assert source[2]>>4==5 and packed[2]>>4==2
        assert abs(len(source)//240-len(packed)//96)<=2
        buffer=(ctypes.c_uint8*length).from_buffer_copy(packed)
        assert decoder.mp3_player_start_once(buffer,length)
        samples=[decoder.mp3_player_render(100) for _ in range(48000)]
        assert min(samples)<-10 and max(samples)>10,slot
        decoder.mp3_player_stop()
print('PASS: all ten packed tracks retain full duration and decode through the actual fixed-point Passport backend')
