#!/usr/bin/env python3
"""Keep all ten complete songs within Passport's 8 MB flash budget."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[3]
LIMIT=0x4A6000
HEADER=struct.Struct('<4sII'+'II'*10)

def ffmpeg():
    binary=os.environ.get('FFMPEG') or shutil.which('ffmpeg')
    if binary:return binary
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        base=getattr(sys,'_base_executable',sys.executable)
        if base!=sys.executable:
            try:
                environment=os.environ.copy()
                for key in ('PYTHONNOUSERSITE','PYTHONHOME','PYTHONPATH','__PYVENV_LAUNCHER__'):
                    environment.pop(key,None)
                return subprocess.check_output([base,'-c','import imageio_ffmpeg;print(imageio_ffmpeg.get_ffmpeg_exe())'],text=True,stderr=subprocess.DEVNULL,env=environment).strip()
            except subprocess.CalledProcessError:
                pass
        raise SystemExit('Install ffmpeg or imageio-ffmpeg for the Passport audio packer.')

def pack(output):
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    binary=ffmpeg();payload=bytearray(b'\xff'*4096);entries=[];tracks=[]
    with tempfile.TemporaryDirectory(prefix='passport-mp3-') as temporary:
        for slot in range(10):
            source=ROOT/'main/assets'/f'track_{slot:02d}.mp3'
            destination=Path(temporary)/source.name
            subprocess.run([binary,'-v','error','-nostdin','-y','-i',str(source),'-map_metadata','-1','-ac','1','-ar','12000','-c:a','libmp3lame','-b:a','16k','-write_xing','0','-id3v2_version','0',str(destination)],check=True)
            data=destination.read_bytes()
            if len(data)<4 or data[0]!=255 or data[1]&0xe6!=0xe2:
                raise ValueError(f'Invalid MP3: {source.name}')
            offset=(len(payload)+4095)//4096*4096
            payload.extend(b'\xff'*(offset-len(payload)));payload.extend(data)
            entries.extend([offset,len(data)])
            tracks.append({'slot':slot,'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'offset':offset,'length':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    if len(payload)>LIMIT:raise ValueError(f'Audio exceeds partition: {len(payload)} > {LIMIT}')
    payload[:HEADER.size]=HEADER.pack(b'GMP3',1,10,*entries)
    output.write_bytes(payload)
    output.with_suffix('.json').write_text(json.dumps({'rate':12000,'channels':1,'bitrate':16000,'complete_songs':True,'bytes':len(payload),'tracks':tracks},indent=2)+'\n')
    print(f'Passport audio: {len(payload)} / {LIMIT} bytes; all ten complete songs')

if __name__=='__main__':pack(sys.argv[1])
