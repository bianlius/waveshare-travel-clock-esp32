"""Verify plant visibility migration in the final legacy settings padding byte."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "main/rom.h").read_bytes().decode("gb18030")
rom = (ROOT / "main/rom.c").read_bytes().decode("gb18030")
record = header[header.index("typedef struct DATASAVEINFO{"):
                header.index("}DATASAVEINFO;") + len("}DATASAVEINFO;")]
old = "\n".join(line for line in record.splitlines() if "homePlantHintConfig" not in line)
old = old.replace("DATASAVEINFO", "OLD_DATASAVEINFO")
start = rom.index("static void migrateHomePlantHintSetting(void){")
migration = rom[start:rom.index("\n}", start) + 2]
defines = "\n".join(line for line in header.splitlines() if line.startswith("#define HOME_PLANT_HINT_"))

harness = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
__DEFINES__
__RECORD__
__OLD__
static DATASAVEINFO dataSaveInfo;
static u8 homePlantHintSaveNeeded;
__MIGRATION__
int main(void){
    assert(sizeof(DATASAVEINFO)==60 && sizeof(OLD_DATASAVEINFO)==60);
    assert(offsetof(DATASAVEINFO,homePlantHintConfig)==59);
    assert(offsetof(DATASAVEINFO,money)==offsetof(OLD_DATASAVEINFO,money));
    assert(offsetof(DATASAVEINFO,alarmClock)==offsetof(OLD_DATASAVEINFO,alarmClock));
    OLD_DATASAVEINFO before={0};
    before.money=1234;before.alarmClock[0]=(450<<4)|2;before.alarmClockWeek[0]=31;
    before.autoRotationEnabled=0;before.homeBgmSelection=8;
    memcpy(&dataSaveInfo,&before,sizeof(before));
    for(unsigned padding=0;padding<256;padding++){
        if(padding==HOME_PLANT_HINT_ON || padding==HOME_PLANT_HINT_OFF) continue;
        dataSaveInfo.homePlantHintConfig=padding;homePlantHintSaveNeeded=0;
        migrateHomePlantHintSetting();
        assert(dataSaveInfo.homePlantHintConfig==HOME_PLANT_HINT_ON && homePlantHintSaveNeeded);
        assert(memcmp(&dataSaveInfo,&before,59)==0);
    }
    dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_OFF;
    unsigned char flash[60];memcpy(flash,&dataSaveInfo,sizeof(flash));
    memset(&dataSaveInfo,0,sizeof(dataSaveInfo));
    memcpy(&dataSaveInfo,flash,sizeof(flash));homePlantHintSaveNeeded=0;
    migrateHomePlantHintSetting();
    assert(dataSaveInfo.homePlantHintConfig==HOME_PLANT_HINT_OFF && !homePlantHintSaveNeeded);
    assert(memcmp(&dataSaveInfo,&before,59)==0);
    dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_ON;
    migrateHomePlantHintSetting();assert(!homePlantHintSaveNeeded);
}
'''
for key, value in {"__DEFINES__": defines, "__RECORD__": record, "__OLD__": old,
                   "__MIGRATION__": migration}.items():
    harness = harness.replace(key, value)
with tempfile.TemporaryDirectory(prefix="gview-plant-setting-") as folder:
    source, binary = Path(folder) / "test.c", Path(folder) / "test"
    source.write_text(harness)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: plant toggle defaults on for old saves; off survives reload without shifting other settings")
