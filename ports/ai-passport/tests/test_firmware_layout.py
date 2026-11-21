"""Reject destructive layouts and incomplete audio before an artifact is delivered."""
from pathlib import Path
import hashlib
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from verify_firmware import ENTRY,verify_firmware_layout

def fixture(cardid=0x356000):
    entries=[(1,2,0x9000,0x6000,'nvs'),(1,1,0xf000,0x1000,'phy_init'),
             (0,0,0x10000,0x300000,'factory'),(0x40,0,0x310000,0x1000,'userdata'),
             (0x50,0,0x311000,0x1000,'wifidata'),(1,2,cardid,0x4000,'cardid'),
             (0x60,0,0x35a000,0x4a6000,'sounddata')]
    table=b''.join(ENTRY.pack(0x50aa,kind,subtype,offset,size,label.encode(),0) for kind,subtype,offset,size,label in entries)
    table+=b'\xeb\xeb'+b'\xff'*14+hashlib.md5(table).digest()
    table=table.ljust(0xc00,b'\xff')
    image=bytearray(b'\xff'*(0x35a000+11*4096))
    image[0x8000:0x8c00]=table
    image[0x10000]=0xe9
    sound=0x35a000
    struct.pack_into('<4sII',image,sound,b'GMP3',1,10)
    for index in range(10):
        struct.pack_into('<II',image,sound+12+8*index,(index+1)*4096,4)
        image[sound+(index+1)*4096:sound+(index+1)*4096+4]=b'\xff\xe3\x18\xc4'
    return image

class FirmwareLayoutTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.build=Path(self.temporary.name)
        (self.build/'FoloToy-AI-Passport.bin').write_bytes(b'\xe9'+b'\0'*31)
    def tearDown(self):self.temporary.cleanup()
    def verify(self,image):return verify_firmware_layout(image,self.build,0x8000,0x10000)
    def test_valid(self):self.assertEqual(len(self.verify(fixture())),7)
    def test_identity_payload(self):
        image=fixture();image[0x356000]=0
        with self.assertRaisesRegex(ValueError,'identity'):self.verify(image)
    def test_moved_identity(self):
        with self.assertRaisesRegex(ValueError,'cardid'):self.verify(fixture(0x350000))
    def test_partition_md5(self):
        image=fixture();image[0x8008]^=1
        with self.assertRaisesRegex(ValueError,'MD5'):self.verify(image)
    def test_missing_audio(self):
        with self.assertRaisesRegex(ValueError,'sound catalog'):self.verify(fixture()[:0x35a000])
    def test_truncated_track(self):
        with self.assertRaisesRegex(ValueError,'track payload'):self.verify(fixture()[:0x35a000+10*4096])
    def test_bad_mp3(self):
        image=fixture();image[0x35a000+4096]=0
        with self.assertRaisesRegex(ValueError,'MP3'):self.verify(image)
    def test_oversized_application(self):
        (self.build/'FoloToy-AI-Passport.bin').write_bytes(b'\xe9'+b'\0'*0x300000)
        with self.assertRaisesRegex(ValueError,'partition limit'):self.verify(fixture())

if __name__=='__main__':unittest.main()
