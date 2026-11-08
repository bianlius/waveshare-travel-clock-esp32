#include "./setting.h"
#include "../../LCD/backLight.h"
#include "../../rom.h"
#include "../../sound.h"

#define SOUND_SET_COUNT 2
#define SOUND_SET_TOP 74
#define SOUND_SET_PITCH 52
#define SOUND_VOLUME_STEP 5
static DisplayObject *soundSetBGObj;
static DisplayObject *soundSetValues[SOUND_SET_COUNT];
static u8 soundSetSelect;
static u8 soundSettingsEditing;
static u8 soundSettingsDirty;
static char volumeText[8];
static char homeBgmSummary[24];

void saveSoundSettingsIfDirty(void){
    clearDelayFunction(&saveSoundSettingsIfDirty);
    if(soundSettingsDirty){
        soundSettingsDirty=0;
        writeData();
    }
}
static void queueSoundSettingsSave(void){
    soundSettingsDirty=1;
    clearDelayFunction(&saveSoundSettingsIfDirty);
    DelayFunction(&saveSoundSettingsIfDirty,2.0f);
}
static void updateSoundValues(void){
    snprintf(volumeText,sizeof(volumeText),"%u %%",dataSaveInfo.volumeNum);
    snprintf(homeBgmSummary,sizeof(homeBgmSummary),"%s",
             dataSaveInfo.homeBgmSelection==HOME_BGM_SILENT ? "静音" :
             soundTrackTitle(dataSaveInfo.homeBgmSelection-1));
}
static void moveSoundSetSelect(void){
    for(u8 i=0;i<SOUND_SET_COUNT;++i)
        soundSetValues[i]->color=i==soundSetSelect ? ORANGE : 0;
    Tween(&soundSetBGObj->y,.5,SOUND_SET_TOP-2+soundSetSelect*SOUND_SET_PITCH,
          0,fs_Cubic_EaseOut);
}
static void updateSoundKeyHints(void){
    if(!soundSettingsEditing){
        setPageUIKeyText(0,"上一项");
        setPageUIMiddleText(soundSetSelect==SOUND_SET_COUNT-1 ? "进入" : "编辑");
        setPageUIKeyText(2,"下一项");
        return;
    }
    u8 decrease=1,increase=1;
    if(soundSetSelect==0){
        decrease=dataSaveInfo.volumeNum>0;
        increase=dataSaveInfo.volumeNum<100;
    }
    setPageUIKeyText(0,decrease ? "减少" : "");
    setPageUIMiddleText("完成");
    setPageUIKeyText(2,increase ? "增加" : "");
}
static u8 stepSoundVolume(u8 current,int direction){
    if(direction<0)
        return current ? ((current-1)/SOUND_VOLUME_STEP)*SOUND_VOLUME_STEP : 0;
    return current<100 ? ((current/SOUND_VOLUME_STEP)+1)*SOUND_VOLUME_STEP : 100;
}
static void adjustSoundSetting(int direction){
    const u8 oldVolume=dataSaveInfo.volumeNum;
    switch(soundSetSelect){
        case 0:
            dataSaveInfo.volumeNum=stepSoundVolume(dataSaveInfo.volumeNum,direction);
            if(oldVolume!=dataSaveInfo.volumeNum) setSoundVal(dataSaveInfo.volumeNum);
            break;
    }
    updateSoundValues();
    updateSoundKeyHints();
    if(oldVolume!=dataSaveInfo.volumeNum)
        queueSoundSettingsSave();
}
static void soundPrevious(void){
    if(soundSettingsEditing){ adjustSoundSetting(-1); return; }
    soundSetSelect=(soundSetSelect+SOUND_SET_COUNT-1)%SOUND_SET_COUNT;
    moveSoundSetSelect();
    updateSoundKeyHints();
}
static void soundNext(void){
    if(soundSettingsEditing){ adjustSoundSetting(1); return; }
    soundSetSelect=(soundSetSelect+1)%SOUND_SET_COUNT;
    moveSoundSetSelect();
    updateSoundKeyHints();
}
static void renderHomeBgmSettingsInit(void);
static void soundConfirm(void){
    if(!soundSettingsEditing && soundSetSelect==SOUND_SET_COUNT-1){
        saveSoundSettingsIfDirty();
        renderHomeBgmSettingsInit();
        return;
    }
    soundSettingsEditing=!soundSettingsEditing;
    updateSoundKeyHints();
}
static void leaveSoundSettings(void){
    saveSoundSettingsIfDirty();
    renderMenuInit();
}
static void soundSettingsEx(void){
    updateSoundKeyHints();
    rendWin(0,0,winW,winH,BGColor);
}
void setSoundInit(void){
    pageNow=PAGE_SOUND_SET;
    BGColor=0xFFFF;
    resetPageView(&soundSettingsEx);
    soundSetSelect=0;
    soundSettingsDirty=0;
    soundSettingsEditing=0;
    soundSetBGObj=newRoundRectDisObj(0x9efc,164,20,78,SOUND_SET_TOP,8,1);
    addChild(0,soundSetBGObj);
    drawPageUITxt("上一项","编辑","下一项");
    const char *labels[SOUND_SET_COUNT]={"音量设置","首页音乐"};
    char *values[SOUND_SET_COUNT]={volumeText,homeBgmSummary};
    for(u8 i=0;i<SOUND_SET_COUNT;++i){
        short y=SOUND_SET_TOP+i*SOUND_SET_PITCH;
        addChild(0,newChar(12,labels[i],0,80,y));
        soundSetValues[i]=newChar(12,values[i],0,177,y);
        addChild(0,soundSetValues[i]);
    }
    updateSoundValues();
    moveSoundSetSelect();
    updateSoundKeyHints();
    setKeyFun(KML,&leaveSoundSettings);
    setKeyFun(KMR,&soundNext);
    setKeyFun(KM1,&soundNext);
    setKeyFun(KM2,&soundConfirm);
    setKeyFun(KM3,&soundPrevious);
}

static u8 homeBgmCursor;
static DisplayObject *homeBgmList;
static char homeBgmNames[HOME_BGM_TRACK_COUNT][32];
static const char *homeBgmChoices[HOME_BGM_TRACK_COUNT+1];
static char homeBgmCurrent[48];
static int homeBgmHintPreview=-2;
static u8 homeBgmHintCursor=255;

static void updateHomeBgmPreviewHint(void){
    const int preview=soundPreviewSelection();
    if(preview==homeBgmHintPreview && homeBgmCursor==homeBgmHintCursor) return;
    const char *right=NULL;
    if(homeBgmCursor && dataSaveInfo.volumeNum)
        right=preview==homeBgmCursor-1 ? "停止试听" : "试听";
    setPageUILongKeyText("返回",right);
    homeBgmHintPreview=preview;
    homeBgmHintCursor=homeBgmCursor;
}
static void returnFromHomeBgmSettings(void){
    soundPreviewStop();
    setSoundInit();
    soundSetSelect=SOUND_SET_COUNT-1;
    moveSoundSetSelect();
    updateSoundKeyHints();
}
static void homeBgmPrevious(void){
    soundPreviewStop();
    homeBgmCursor=(homeBgmCursor+HOME_BGM_TRACK_COUNT)%(HOME_BGM_TRACK_COUNT+1);
    setMenuListY(homeBgmList,homeBgmCursor);
    updateHomeBgmPreviewHint();
}
static void homeBgmNext(void){
    soundPreviewStop();
    homeBgmCursor=(homeBgmCursor+1)%(HOME_BGM_TRACK_COUNT+1);
    setMenuListY(homeBgmList,homeBgmCursor);
    updateHomeBgmPreviewHint();
}
static void homeBgmPreviewToggle(void){
    if(!homeBgmCursor || !dataSaveInfo.volumeNum) return;
    if(soundPreviewSelection()==homeBgmCursor-1)
        soundPreviewStop();
    else if(!soundPreviewStart(homeBgmCursor-1))
        showTips("音源不可用",1.5f);
    updateHomeBgmPreviewHint();
}
static void homeBgmChoose(void){
    soundPreviewStop();
    if(dataSaveInfo.homeBgmSelection!=homeBgmCursor){
        dataSaveInfo.homeBgmSelection=homeBgmCursor;
        if(homeBgmCursor) dataSaveInfo.homeBgmPreferredSelection=homeBgmCursor;
        writeData();
    }
    returnFromHomeBgmSettings();
}
static void homeBgmSettingsEx(void){
    updateHomeBgmPreviewHint();
    rendWin(0,0,winW,winH,BGColor);
}
static void renderHomeBgmSettingsInit(void){
    soundPreviewStop();
    pageNow=PAGE_BGM_SET;
    BGColor=0xFFFF;
    resetPageView(&homeBgmSettingsEx);
    homeBgmCursor=dataSaveInfo.homeBgmSelection<=HOME_BGM_TRACK_COUNT ?
                  dataSaveInfo.homeBgmSelection : HOME_BGM_SILENT;
    homeBgmHintPreview=-2;
    homeBgmHintCursor=255;
    drawPageUITxt("上一项","选用","下一项");
    homeBgmChoices[0]="静音";
    for(u8 i=0;i<HOME_BGM_TRACK_COUNT;++i){
        snprintf(homeBgmNames[i],sizeof(homeBgmNames[i]),"%u %s",
                 (unsigned)(i+1),soundTrackTitle(i));
        homeBgmChoices[i+1]=homeBgmNames[i];
    }
    snprintf(homeBgmCurrent,sizeof(homeBgmCurrent),"当前: %s",
             homeBgmCursor==HOME_BGM_SILENT ? "静音" :
             soundTrackTitle(homeBgmCursor-1));
    addChild(0,newChar(12,homeBgmCurrent,0,52,34));
    homeBgmList=addMenuList(homeBgmChoices,12,0xFFFF,0x04BE,
                            HOME_BGM_TRACK_COUNT+1,220,54,WS_UI_FOOTER_TOP,1);
    homeBgmList->parent->x=48;
    setMenuListY(homeBgmList,homeBgmCursor);
    updateHomeBgmPreviewHint();
    setKeyFun(KML,&returnFromHomeBgmSettings);
    setKeyFun(KMR,&homeBgmPreviewToggle);
    setKeyFun(KM3,&homeBgmPrevious);
    setKeyFun(KM2,&homeBgmChoose);
    setKeyFun(KM1,&homeBgmNext);
}

static DisplayObject *systemValue;
static u8 systemEditing;
static u8 systemDirty;
static char tempOffsetText[8];
void saveSystemSettingsIfDirty(void){
    clearDelayFunction(&saveSystemSettingsIfDirty);
    if(systemDirty){ systemDirty=0; writeData(); }
}
static void updateSystemSettings(void){
    signed char value=dataSaveInfo.tempOffset;
    snprintf(tempOffsetText,sizeof(tempOffsetText),"%s%d",value>0?"+":"",value);
    systemValue->color=systemEditing ? ORANGE : 0;
    setPageUIKeyText(0,systemEditing && value>-5 ? "减少" : "");
    setPageUIMiddleText(systemEditing ? "完成" : "编辑");
    setPageUIKeyText(2,systemEditing && value<5 ? "增加" : "");
}
static void systemPrevious(void){
    if(systemEditing && dataSaveInfo.tempOffset>-5){
        --dataSaveInfo.tempOffset;
        systemDirty=1;
        clearDelayFunction(&saveSystemSettingsIfDirty);
        DelayFunction(&saveSystemSettingsIfDirty,2.0f);
        updateSystemSettings();
    }
}
static void systemNext(void){
    if(systemEditing && dataSaveInfo.tempOffset<5){
        ++dataSaveInfo.tempOffset;
        systemDirty=1;
        clearDelayFunction(&saveSystemSettingsIfDirty);
        DelayFunction(&saveSystemSettingsIfDirty,2.0f);
        updateSystemSettings();
    }
}
static void systemConfirm(void){
    systemEditing=!systemEditing;
    updateSystemSettings();
}
void leaveSystemSettings(void){
    saveSystemSettingsIfDirty();
    renderMenuInit();
}
static void systemSettingsEx(void){
    rendWin(0,0,winW,winH,BGColor);
}
void setSystemInit(void){
    pageNow=PAGE_SYSTEM_SET;
    BGColor=0xFFFF;
    resetPageView(&systemSettingsEx);
    systemEditing=0;
    systemDirty=0;
    drawPageUITxt("","编辑","");
    addChild(0,newRoundRectDisObj(0x9efc,164,20,78,44,8,1));
    addChild(0,newChar(12,"温度补偿",0,80,44));
    systemValue=newChar(12,tempOffsetText,0,177,44);
    addChild(0,systemValue);
    addChild(moveClipMaxNum-1,newNativePicSingle(&duc,0,218,44));
    addChild(0,newChar(12,"系统版本 v2.4",0,80,84));
    updateSystemSettings();
    setKeyFun(KML,&leaveSystemSettings);
    setKeyFun(KMR,&systemNext);
    setKeyFun(KM1,&systemNext);
    setKeyFun(KM2,&systemConfirm);
    setKeyFun(KM3,&systemPrevious);
}

// The brightness menu opens the five daily schedule entries directly.
void setBGLightInit(void){
    setBGLightScheduleInit();
}
