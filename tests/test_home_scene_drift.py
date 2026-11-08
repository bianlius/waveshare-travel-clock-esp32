"""Exercise the production home scene and status drift without drawing hardware."""

from pathlib import Path
import subprocess
import tempfile


source = (Path(__file__).resolve().parents[1] / "main/PAGE/HOME/home.c").read_bytes().decode("gb18030")
constant_start = source.index("#define HOME_SCENE_DRIFT_X ")
constant = source[constant_start:source.index("static uint32_t homeDriftFrame;", constant_start)]
start = source.index("static void driftHomeScene(void){")
brace = source.index("{", start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
production = source[start:end]

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
typedef struct { short x, y; } MovieClip;
static struct { MovieClip movieclipArr[10]; } stage;
static const u8 moveClipMaxNum=10;
static uint32_t homeDriftFrame;
static short homeSceneOffsetX;
static short homeUiOffsetX;
static short homeUiOffsetY;
static u8 fps=25;
static u8 hour=3;
'''

scenarios = r'''
int main(void){
    stage.movieclipArr[2].x=-42; // Scrolling ground has its own position.
    int minimum=100, maximum=-100, previous=0;
    int uiMinimum=100, uiMaximum=-100, uiYMaximum=-100;
    // A full 20-minute cycle at 25 frames per second.
    for(int frame=0;frame<30000;frame++){
        if(frame==15000) hour=12; // Continue moving through daytime.
        driftHomeScene();
        int shift=stage.movieclipArr[3].x;
        if(shift<minimum) minimum=shift;
        if(shift>maximum) maximum=shift;
        assert(shift-previous>=-1 && shift-previous<=1);
        for(int layer=0;layer<9;layer++)
            assert(stage.movieclipArr[layer].x==shift+(layer==2 ? -42 : 0));
        int uiX=stage.movieclipArr[9].x;
        int uiY=stage.movieclipArr[9].y;
        if(uiX<uiMinimum) uiMinimum=uiX;
        if(uiX>uiMaximum) uiMaximum=uiX;
        if(uiY>uiYMaximum) uiYMaximum=uiY;
        assert(uiX>=-3 && uiX<=0 && uiY>=0 && uiY<=2);
        // Edge-aligned weather and bottom status content remain visible.
        assert(256+24+uiX<=280);
        assert(221+16+uiY<=240);
        if(frame==20000) assert(shift<0);
        previous=shift;
    }
    assert(minimum==-4 && maximum==0);
    assert(uiMinimum==-3 && uiMaximum==0 && uiYMaximum==2);
    assert(stage.movieclipArr[3].x==0);
    assert(stage.movieclipArr[2].x==-42);
    assert(stage.movieclipArr[9].x==0 && stage.movieclipArr[9].y==0);
    puts("PASS: home art and status drift slowly in bounds across day and night");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-home-drift-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(harness + constant + production + scenarios)
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=address,undefined", str(test), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
