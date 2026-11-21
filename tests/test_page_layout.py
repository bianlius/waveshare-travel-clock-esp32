"""Keep page geometry within each target and reject old square viewports."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
for path in (ROOT / "main/PAGE").rglob("*.c"):
    source = path.read_bytes().decode("gb18030")
    for call in re.findall(r"LCD_SetPageViewport\(([^;]+)\);", source):
        args = [arg.strip() for arg in call.split(",")]
        assert len(args) == 6, (path, call)
        assert args[4] in ("WS_UI_WIDTH", "WS_HOME_SIZE"), (path, call)

harness = r'''
#include <assert.h>
#include <stdio.h>
#include "page_layout.h"
int main(void){
    assert(PAGE_CONTENT_LEFT>=WS_UI_LEFT && PAGE_CONTENT_RIGHT<=WS_UI_RIGHT);
    assert(PAGE_FORM_LEFT>=WS_UI_LEFT && PAGE_FORM_LEFT+PAGE_FORM_WIDTH<=WS_UI_RIGHT);
    assert(PAGE_FORM_LABEL_LEFT+80<PAGE_FORM_VALUE_LEFT);
    assert(PAGE_FORM_VALUE_LEFT+96<=WS_UI_RIGHT);
    assert(PAGE_FORM_NUMBER_LEFT+96<=WS_UI_RIGHT);
    assert(PAGE_SYSTEM_VALUE_LEFT+32<=WS_UI_RIGHT);
    assert(PAGE_SYSTEM_DEGREE_LEFT+16<=WS_UI_RIGHT);
    assert(PAGE_CITY_PROVINCE_LEFT>=WS_UI_LEFT);
    assert(PAGE_CITY_PROVINCE_LEFT+PAGE_CITY_COLUMN_WIDTH<=PAGE_CITY_CITY_LEFT);
    assert(PAGE_CITY_CITY_LEFT+PAGE_CITY_COLUMN_WIDTH<=WS_UI_RIGHT);
    assert(PAGE_CITY_TEXT_WIDTH+6<=PAGE_CITY_COLUMN_WIDTH);
    assert(PAGE_CLOCK_NUMBER_LEFT>=WS_UI_LEFT);
    assert(PAGE_CLOCK_HOUR_LEFT+16<=PAGE_CLOCK_COLON_LEFT);
    // The colon's ink occupies the centre of its 8px font cell.
    assert(PAGE_CLOCK_COLON_LEFT+6<=PAGE_CLOCK_MINUTE_LEFT);
    assert(PAGE_CLOCK_MINUTE_LEFT+16<=PAGE_CLOCK_RING_LEFT);
    assert(PAGE_CLOCK_RING_LEFT+96<=WS_UI_RIGHT);
    assert(PAGE_CLOCK_WEEK_LEFT+6*PAGE_CLOCK_WEEK_PITCH+18<=WS_UI_RIGHT);
    assert(7*PAGE_CALENDAR_COLUMN_WIDTH<=PAGE_CONTENT_WIDTH);
    assert(PAGE_CONTENT_WIDTH-7*PAGE_CALENDAR_COLUMN_WIDTH<7);
    assert(PAGE_WEATHER_FIRST_DAY+PAGE_WEATHER_COLUMNS<=3);
    assert(PAGE_WEATHER_CARD_WIDTH>=88);
    assert(6+(PAGE_WEATHER_COLUMNS-1)*PAGE_WEATHER_CARD_PITCH+PAGE_WEATHER_CARD_WIDTH<=WS_UI_RIGHT-WS_UI_LEFT-6);
    puts("PASS: target-owned content, forms, alarm columns, calendar and city geometry");
}
'''
with tempfile.TemporaryDirectory(prefix="gview-page-layout-") as folder:
    source = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    source.write_text(harness)
    for board, left, width in (("waveshare",40,240),("ai-passport",0,320)):
        subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                        f"-DWS_UI_LEFT={left}",f"-DWS_UI_RIGHT={left+width}",
                        f"-DWS_UI_WIDTH={width}",
                        "-I"+str(ROOT / "main/PAGE"),
                        "-I"+str(ROOT / "ports" / board / "include"),
                        str(source),"-o",str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
print("PASS: shared pages no longer force a 240-pixel viewport")
