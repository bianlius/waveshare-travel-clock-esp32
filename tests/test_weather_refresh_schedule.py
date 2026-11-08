"""The weather scheduler must tolerate missed second-zero ticks."""

from pathlib import Path
import subprocess
import tempfile


source = (Path(__file__).resolve().parents[1] / "main/NET/RTC_Internet.c").read_bytes().decode("gb18030")
start = source.index("    // Fetch once on each five-minute boundary")
end = source.index("\n    refrushDayTime();", start)
scheduler = source[start:end]

harness = r'''
#include <assert.h>
#include <stdio.h>
#include <time.h>

static int requests;
static void weatherForecastRequest(int clear_previous_city){
    assert(clear_previous_city == 0);
    ++requests;
}
static void tick(time_t tt){
__SCHEDULER__
}
int main(void){
    const time_t base = 1704067200;
    tick(100);              // RTC not synchronized yet.
    tick(base + 42);        // The initial fetch is made during startup.
    assert(requests == 0);
    tick(base + 300 + 3);   // Missed second zero, still refresh.
    assert(requests == 1);
    tick(base + 300 + 50);  // No duplicate in the same slot.
    assert(requests == 1);
    tick(base + 900 + 12);  // A delayed task catches the later slot once.
    assert(requests == 2);
    tick(base + 600 + 20);  // Resynchronize after a clock correction.
    assert(requests == 2);
    tick(base + 900 + 1);
    assert(requests == 3);
    puts("PASS: weather refreshes once per crossed five-minute slot");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-weather-schedule-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__SCHEDULER__", scheduler), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
