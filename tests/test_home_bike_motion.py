"""Check the production home bicycle motion and frame synchronization."""

from pathlib import Path
import subprocess
import tempfile


source = (Path(__file__).resolve().parents[1] / "main/PAGE/HOME/home.c").read_bytes().decode("gb18030")
start = source.index("static void moveHomeBike(void){")
brace = source.index("{", start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
production = source[start:end]

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
#define HOME_BIKE_PARTS 9
#define HOME_BIKE_WHEEL_COUNT 2
#define HOME_BIKE_TRAVEL 24
typedef struct { uint8_t totallFrame, currentFrame; } DisObjAction;
typedef struct { short x; DisObjAction *ac; } DisplayObject;
static DisplayObject *homeBikeParts[HOME_BIKE_PARTS];
static short homeBikeBaseX[HOME_BIKE_PARTS];
static u8 homeBikePartCount;
static uint32_t homeBikeFrame;
static uint8_t homeWheelFrame,homeWheelTicks;
static uint8_t homeRiderFrame,homeRiderTicks;
static u8 fps=25;
static int hour=12;
static void AcGotoAndStop(DisObjAction *action,uint8_t frame){
    action->currentFrame=frame;
}
'''

scenarios = r'''
int main(void){
    DisObjAction actions[4]={{4,0},{4,0},{4,0},{4,0}};
    DisplayObject parts[4]={{120,&actions[0]},{150,&actions[1]},
                            {175,&actions[2]},{190,&actions[3]}};
    homeBikePartCount=4;
    for(int i=0;i<4;i++){
        homeBikeParts[i]=&parts[i];
        homeBikeBaseX[i]=parts[i].x;
    }
    int offsets[201], wheel_frames[201];
    int minimum=100,maximum=-100;
    int forwardChanges=0,returnChanges=0;
    int pedalChanges=0,previousPedal=0;
    for(int i=0;i<201;i++){
        moveHomeBike();
        int offset=parts[0].x-120;
        offsets[i]=offset;
        wheel_frames[i]=actions[0].currentFrame;
        assert(parts[1].x-150==offset && parts[2].x-175==offset && parts[3].x-190==offset);
        assert(actions[0].currentFrame==actions[1].currentFrame);
        assert(actions[2].currentFrame==actions[3].currentFrame);
        if(i){
            int step=(wheel_frames[i]-wheel_frames[i-1]+4)%4;
            assert(step<=1); // Wheels never reverse when the rider freewheels.
            if(i<50) forwardChanges+=step;
            else if(i>=50 && i<150) returnChanges+=step;
        }
        if(i && i<50) pedalChanges+=(actions[2].currentFrame-previousPedal+4)%4;
        previousPedal=actions[2].currentFrame;
        if(i>=50 && i<150) assert(actions[2].currentFrame==0); // Freewheel backward.
        if(offset<minimum) minimum=offset;
        if(offset>maximum) maximum=offset;
    }
    assert(offsets[0]==0 && offsets[200]==0);
    assert(minimum==-24 && maximum==24);
    int near_turn=0,mid_ride=0;
    for(int i=51;i<71;i++) near_turn+=(offsets[i]!=offsets[i-1]);
    for(int i=91;i<111;i++) mid_ride+=(offsets[i]!=offsets[i-1]);
    assert(mid_ride>near_turn); // Speed rises after the turn.
    assert(offsets[75]<offsets[50] && offsets[125]<offsets[100]);
    assert(forwardChanges>0 && returnChanges>0 && forwardChanges*2>returnChanges);
    assert(forwardChanges>=24 && forwardChanges<=25); // Previously 16 changes in this span.
    assert(pedalChanges>=15 && pedalChanges<=17); // Original pedalling cadence.
    assert(returnChanges>=16 && returnChanges<=17); // Original return wheel speed.
    int wheel_changed=0;
    for(int i=51;i<150;i++) wheel_changed|=wheel_frames[i]!=wheel_frames[i-1];
    assert(wheel_changed); // Wheels still roll while the feet stay fixed.
    hour=3;
    moveHomeBike();
    assert(parts[0].x==120 && homeBikeFrame==0);
    assert(actions[2].currentFrame==0);
    assert(actions[0].currentFrame==0 && homeWheelTicks==0);
    assert(homeRiderFrame==0 && homeRiderTicks==0);
    hour=12;
    moveHomeBike();
    assert(parts[0].x==120); // Restart from centre after sleep.
    puts("PASS: faster forward wheels, original pedal/return cadence, synchronized clothing, and sleep reset");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-home-bike-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(harness + production + scenarios)
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=address,undefined", str(test), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
