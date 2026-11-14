#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#include "simulator.h"
#include <unistd.h>
#include <signal.h>
#include <errno.h>

static AVAudioPlayer *player;
static BOOL soundEnabled=NO;
static float soundVolume=0.8f;
static NSString *captureDirectory;
static NSString *windowCapturePath;
static BOOL replaceRunning;

void desktop_audio_play(int slot,int loop,float volume){
    @autoreleasepool {
        [player stop];
        NSString *path=[NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:
            [NSString stringWithFormat:@"track_%02d.mp3",slot]];
        player=[[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path] error:NULL];
        soundVolume=volume;player.volume=soundEnabled?volume:0;player.numberOfLoops=loop?-1:0;[player play];
    }
}
void desktop_audio_stop(void){[player stop];player=nil;}
void desktop_audio_volume(float volume){soundVolume=volume;player.volume=soundEnabled?volume:0;}

static CGImageRef gameImage(void){
    const uint32_t *data=desktop_pixels();
    CGDataProviderRef provider=CGDataProviderCreateWithData(NULL,data,240*240*4,NULL);
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGImageRef image=CGImageCreate(240,240,8,32,240*4,space,
        kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,provider,NULL,false,kCGRenderingIntentDefault);
    CGColorSpaceRelease(space);CGDataProviderRelease(provider);return image;
}
static void capture(const char *name){
    CGImageRef image=gameImage();NSBitmapImageRep *rep=[[NSBitmapImageRep alloc] initWithCGImage:image];
    NSData *png=[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    [png writeToFile:[captureDirectory stringByAppendingPathComponent:
        [NSString stringWithFormat:@"%s.png",name]] atomically:YES];CGImageRelease(image);
}

@interface SimulatorController : NSObject<NSApplicationDelegate,NSWindowDelegate>
@property NSWindow *window;
@property NSView *canvas;
@property NSTextField *status;
@property NSTextField *companionName;
@property NSTimer *timer;
@property BOOL paused;
- (void)frame:(NSTimer *)timer;
- (void)openPage:(NSButton *)button;
- (void)physical:(NSButton *)button;
- (void)control:(NSButton *)button;
- (void)cycleCompanion:(NSButton *)button;
@end

@interface Canvas : NSView
@property SimulatorController *controller;
@property NSPoint startPoint;
@property NSMutableDictionary<NSNumber *,NSNumber *> *downTimes;
@end
@implementation Canvas
- (BOOL)isFlipped{return YES;}
- (BOOL)acceptsFirstResponder{return YES;}
- (void)drawRect:(NSRect)rect{
    (void)rect;CGImageRef image=gameImage();NSImage *bitmap=[[NSImage alloc] initWithCGImage:image size:NSMakeSize(240,240)];
    [bitmap drawInRect:self.bounds fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1
        respectFlipped:YES hints:@{NSImageHintInterpolation:@(NSImageInterpolationNone)}];CGImageRelease(image);
}
- (void)mouseDown:(NSEvent *)event{
    [self.window makeFirstResponder:self];NSPoint p=[self convertPoint:event.locationInWindow fromView:nil];
    self.startPoint=NSMakePoint(p.x*240/self.bounds.size.width,p.y*240/self.bounds.size.height);
}
- (void)mouseUp:(NSEvent *)event{
    NSPoint p=[self convertPoint:event.locationInWindow fromView:nil];
    desktop_touch(self.startPoint.x,self.startPoint.y,p.x*240/self.bounds.size.width,p.y*240/self.bounds.size.height);
    [self.controller frame:nil];
}
- (void)keyDown:(NSEvent *)event{
    if(event.isARepeat)return;
    unsigned key=event.keyCode;
    if(key==123 || key==124 || key==36 || key==76){
        if(!self.downTimes)self.downTimes=[NSMutableDictionary new];
        self.downTimes[@(key)]=@([NSDate timeIntervalSinceReferenceDate]);return;
    }
    NSString *c=event.charactersIgnoringModifiers.lowercaseString;
    if([c isEqualToString:@" "]){self.controller.paused=!self.controller.paused;return;}
    if(key==53){desktop_key(0,1);return;}
    if(c.length==1 && [@"12345678" containsString:c]){desktop_open([c intValue]-1);return;}
    if([c isEqualToString:@"d"])desktop_day();
    if([c isEqualToString:@"w"])desktop_weather();
    if([c isEqualToString:@"["])desktop_cycle_companion(-1);
    if([c isEqualToString:@"]"])desktop_cycle_companion(1);
}
- (void)keyUp:(NSEvent *)event{
    NSNumber *start=self.downTimes[@(event.keyCode)];if(!start)return;
    BOOL held=([NSDate timeIntervalSinceReferenceDate]-start.doubleValue)>0.65 || (event.modifierFlags&NSEventModifierFlagShift);
    desktop_key(event.keyCode==123?0:event.keyCode==124?2:1,held);
    [self.downTimes removeObjectForKey:@(event.keyCode)];[self.controller frame:nil];
}
@end

@interface HoldButton : NSButton
@property NSTimeInterval pressedAt;
@end
@implementation HoldButton
- (void)mouseDown:(NSEvent *)event{self.pressedAt=[NSDate timeIntervalSinceReferenceDate];[super mouseDown:event];}
@end

@interface FlippedView : NSView
@end
@implementation FlippedView
- (BOOL)isFlipped{return YES;}
- (void)drawRect:(NSRect)rect{[[NSColor colorWithWhite:0.94 alpha:1] setFill];NSRectFill(rect);}
@end

@implementation SimulatorController
- (NSButton *)button:(NSString *)title frame:(NSRect)frame tag:(NSInteger)tag action:(SEL)action held:(BOOL)held{
    NSButton *b=held?[[HoldButton alloc] initWithFrame:frame]:[[NSButton alloc] initWithFrame:frame];
    b.title=title;b.tag=tag;b.bezelStyle=NSBezelStyleRounded;b.font=[NSFont systemFontOfSize:12];
    b.attributedTitle=[[NSAttributedString alloc] initWithString:title attributes:
        @{NSForegroundColorAttributeName:NSColor.blackColor,NSFontAttributeName:b.font}];
    b.target=self;b.action=action;[self.window.contentView addSubview:b];return b;
}
- (void)applicationDidFinishLaunching:(NSNotification *)note{
    (void)note;
    if(replaceRunning){
        for(NSRunningApplication *other in [NSRunningApplication runningApplicationsWithBundleIdentifier:@"cn.gview.travel.simulator"]){
            pid_t pid=other.processIdentifier;
            if(pid==getpid())continue;
            if(![other terminate]){fprintf(stderr,"Simulator did not accept a save-and-quit request.\n");[NSApp terminate:nil];return;}
            NSDate *deadline=[NSDate dateWithTimeIntervalSinceNow:5];
            while(kill(pid,0)==0 && deadline.timeIntervalSinceNow>0)usleep(50000);
            if(kill(pid,0)==0){fprintf(stderr,"Existing simulator is still closing.\n");[NSApp terminate:nil];return;}
        }
    }
    NSArray *dirs=NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,NSUserDomainMask,YES);
    NSString *state=[dirs.firstObject stringByAppendingPathComponent:@"GVIEW Travel Simulator"];
    [[NSFileManager defaultManager] createDirectoryAtPath:state withIntermediateDirectories:YES attributes:nil error:NULL];
    desktop_init(state.fileSystemRepresentation,NSBundle.mainBundle.resourcePath.fileSystemRepresentation);
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,512,746)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"GVIEW 旅行时钟 · 电脑模拟";self.window.delegate=self;
    self.window.appearance=[NSAppearance appearanceNamed:NSAppearanceNameAqua];
    self.window.contentView=[[FlippedView alloc] initWithFrame:NSMakeRect(0,0,512,746)];
    NSArray *tabs=@[@"主页",@"森林",@"商店",@"花园",@"设置",@"日历",@"时钟",@"天气"];
    for(int i=0;i<8;i++)[self button:tabs[i] frame:NSMakeRect(16+i*60,12,57,28) tag:i action:@selector(openPage:) held:NO];
    Canvas *canvas=[[Canvas alloc] initWithFrame:NSMakeRect(16,52,480,480)];canvas.controller=self;
    self.canvas=canvas;[self.window.contentView addSubview:canvas];
    NSArray *keys=@[@"◀ 左键",@"● 中键",@"右键 ▶"];
    for(int i=0;i<3;i++)[self button:keys[i] frame:NSMakeRect(16+i*162,545,156,30) tag:i action:@selector(physical:) held:YES];
    [self button:@"◀ 上一只" frame:NSMakeRect(16,584,132,28) tag:-1 action:@selector(cycleCompanion:) held:NO];
    [self button:@"下一只 ▶" frame:NSMakeRect(364,584,132,28) tag:1 action:@selector(cycleCompanion:) held:NO];
    self.companionName=[NSTextField labelWithString:@""];
    self.companionName.frame=NSMakeRect(152,588,208,22);self.companionName.alignment=NSTextAlignmentCenter;
    self.companionName.font=[NSFont systemFontOfSize:13];[self.window.contentView addSubview:self.companionName];
    NSArray *controls=@[@"切换时段",@"切换天气",@"暂停",@"声音：关",@"+5万金币"];
    for(int i=0;i<5;i++)[self button:controls[i] frame:NSMakeRect(16+i*96,626,93,28) tag:i action:@selector(control:) held:NO];
    self.status=[NSTextField labelWithString:@""];self.status.frame=NSMakeRect(20,662,476,24);
    self.status.font=[NSFont systemFontOfSize:13];[self.window.contentView addSubview:self.status];
    NSTextField *hint=[NSTextField labelWithString:@"← / → / Enter：短按    按住或 Shift：长按    Esc：返回"];
    hint.frame=NSMakeRect(20,692,480,20);hint.font=[NSFont systemFontOfSize:11];[self.window.contentView addSubview:hint];
    NSTextField *info=[NSTextField labelWithString:@"本机模拟存档 · 快捷切换免金币 · 网络、天气和传感器为模拟值"];
    info.frame=NSMakeRect(20,717,480,18);info.font=[NSFont systemFontOfSize:11];info.textColor=NSColor.secondaryLabelColor;
    [self.window.contentView addSubview:info];
    NSMenu *menu=[NSMenu new];NSMenuItem *appItem=[NSMenuItem new];[menu addItem:appItem];
    NSMenu *appMenu=[NSMenu new];[appMenu addItemWithTitle:@"退出模拟器" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu=appMenu;NSApp.mainMenu=menu;
    [self.window center];[self.window makeKeyAndOrderFront:nil];[self.window makeFirstResponder:canvas];
    [NSApp activateIgnoringOtherApps:YES];
    self.timer=[NSTimer scheduledTimerWithTimeInterval:0.04 target:self selector:@selector(frame:) userInfo:nil repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:self.timer forMode:NSRunLoopCommonModes];[self frame:nil];
    if(windowCapturePath){
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),dispatch_get_main_queue(),^{
            NSView *view=self.window.contentView;[view displayIfNeeded];
            NSBitmapImageRep *rep=[view bitmapImageRepForCachingDisplayInRect:view.bounds];
            [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
            [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:windowCapturePath atomically:YES];
        });
    }
}
- (void)frame:(NSTimer *)timer{
    (void)timer;if(!self.paused)desktop_tick();
    self.status.stringValue=[NSString stringWithUTF8String:desktop_status()];[self.canvas setNeedsDisplay:YES];
    self.companionName.stringValue=[NSString stringWithUTF8String:desktop_companion_label()];
}
- (void)openPage:(NSButton *)button{desktop_open((int)button.tag);[self.window makeFirstResponder:self.canvas];[self frame:nil];}
- (void)cycleCompanion:(NSButton *)button{
    desktop_cycle_companion((int)button.tag);[self.window makeFirstResponder:self.canvas];[self frame:nil];
}
- (void)physical:(NSButton *)button{
    HoldButton *hold=[button isKindOfClass:HoldButton.class]?(HoldButton *)button:nil;
    BOOL held=hold.pressedAt>0 && ([NSDate timeIntervalSinceReferenceDate]-hold.pressedAt)>0.65;
    hold.pressedAt=0;
    desktop_key((int)button.tag,held);[self.window makeFirstResponder:self.canvas];[self frame:nil];
}
- (void)control:(NSButton *)button{
    switch(button.tag){
        case 0:desktop_day();break;
        case 1:desktop_weather();break;
        case 2:self.paused=!self.paused;button.title=self.paused?@"继续":@"暂停";break;
        case 3:soundEnabled=!soundEnabled;player.volume=soundEnabled?soundVolume:0;button.title=soundEnabled?@"声音：开":@"声音：关";break;
        case 4:desktop_credit();break;
    }
    [self.window makeFirstResponder:self.canvas];[self frame:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender{(void)sender;return YES;}
- (void)applicationWillTerminate:(NSNotification *)note{(void)note;[self.timer invalidate];desktop_shutdown();}
@end

int main(int argc,char **argv){
    @autoreleasepool {
        if(argc==3 && strcmp(argv[1],"--smoke")==0){
            captureDirectory=[NSString stringWithUTF8String:argv[2]];
            [[NSFileManager defaultManager] createDirectoryAtPath:captureDirectory withIntermediateDirectories:YES attributes:nil error:NULL];
            NSString *state=[captureDirectory stringByAppendingPathComponent:@"state"];
            [[NSFileManager defaultManager] createDirectoryAtPath:state withIntermediateDirectories:YES attributes:nil error:NULL];
            desktop_init(state.fileSystemRepresentation,NSBundle.mainBundle.resourcePath.fileSystemRepresentation);
            int result=desktop_smoke(capture);desktop_shutdown();return result;
        }
        for(int i=1;i<argc;i++){
            if(strcmp(argv[i],"--replace-running")==0)replaceRunning=YES;
            if(strcmp(argv[i],"--capture-window")==0 && i+1<argc)windowCapturePath=[NSString stringWithUTF8String:argv[++i]];
        }
        NSApplication *app=NSApplication.sharedApplication;[app setActivationPolicy:NSApplicationActivationPolicyRegular];
        SimulatorController *controller=[SimulatorController new];app.delegate=controller;[app run];
    }
    return 0;
}
