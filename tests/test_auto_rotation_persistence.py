"""Check migration of the saved rotation flag without shifting old data."""

from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = (root / "main/rom.h").read_bytes().decode("gb18030")
rom = (root / "main/rom.c").read_bytes().decode("gb18030")
record = header[header.index("typedef struct DATASAVEINFO{"):
                header.index("}DATASAVEINFO;") + len("}DATASAVEINFO;")]
old_record = (record.replace("typedef struct DATASAVEINFO{", "typedef struct OLD_DATASAVEINFO{")
              .replace("    u8 autoRotationConfigVersion;\n", "")
              .replace("    u8 autoRotationEnabled;\n", "")
              .replace("}DATASAVEINFO;", "}OLD_DATASAVEINFO;"))
start = rom.index("static void migrateAutoRotationSetting(void){")
end = rom.index("\n}", start) + 2
migration = rom[start:end]
version = header.split("#define AUTO_ROTATION_CONFIG_VERSION ", 1)[1].splitlines()[0]

harness = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define AUTO_ROTATION_CONFIG_VERSION __VERSION__
__RECORD__
__OLD_RECORD__
static DATASAVEINFO dataSaveInfo;
static u8 autoRotationSaveNeeded;
__MIGRATION__
int main(void){
    // The two new bytes occupy the old tail padding; saved fields retain offsets.
    assert(sizeof(DATASAVEINFO)==sizeof(OLD_DATASAVEINFO));
    assert(offsetof(DATASAVEINFO,homeBgmPreferredSelection)==
           offsetof(OLD_DATASAVEINFO,homeBgmPreferredSelection));
    assert(offsetof(DATASAVEINFO,autoRotationConfigVersion)==
           offsetof(OLD_DATASAVEINFO,homeBgmPreferredSelection)+1);
    assert(sizeof(DATASAVEINFO)<1024);

    dataSaveInfo.money=1234;
    dataSaveInfo.homeBgmSelection=7;
    // Old firmware wrote zero in the tail padding.
    dataSaveInfo.autoRotationConfigVersion=0;
    dataSaveInfo.autoRotationEnabled=0;
    migrateAutoRotationSetting();
    assert(dataSaveInfo.autoRotationConfigVersion==AUTO_ROTATION_CONFIG_VERSION);
    assert(dataSaveInfo.autoRotationEnabled==1 && autoRotationSaveNeeded==1);
    assert(dataSaveInfo.money==1234 && dataSaveInfo.homeBgmSelection==7);

    autoRotationSaveNeeded=0;
    dataSaveInfo.autoRotationEnabled=0;
    migrateAutoRotationSetting();
    assert(dataSaveInfo.autoRotationEnabled==0 && autoRotationSaveNeeded==0);

    dataSaveInfo.autoRotationEnabled=2;
    migrateAutoRotationSetting();
    assert(dataSaveInfo.autoRotationEnabled==1 && autoRotationSaveNeeded==1);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="gview-auto-rotation-save-") as folder:
    source = Path(folder) / "migration.c"
    binary = Path(folder) / "migration"
    source.write_text(harness.replace("__VERSION__", version)
                      .replace("__RECORD__", record)
                      .replace("__OLD_RECORD__", old_record)
                      .replace("__MIGRATION__", migration))
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(source), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)
