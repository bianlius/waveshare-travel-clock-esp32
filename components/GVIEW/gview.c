/***
 * v0.2 ESP32-C3优化版本
 * 增加硬件加速和内存优化
 */

#include "font.h" 
#include "GB2312.h"
#include "GB2312_16.h"
#include "gview.h"
#include "scale_1bit.h"
#include "sdkconfig.h"
#include <assert.h>
#include <stdlib.h>

static GVIEWNativeTextBegin nativeTextBegin;
static GVIEWNativeTextDraw nativeTextDraw;
static GVIEWNativeIconBegin nativeIconBegin;
static GVIEWNativeIconDraw nativeIconDraw;

void gviewSetNativeIconHooks(GVIEWNativeIconBegin begin, GVIEWNativeIconDraw draw){
    nativeIconBegin = begin;
    nativeIconDraw = draw;
}

void gviewSetNativeTextHooks(GVIEWNativeTextBegin begin, GVIEWNativeTextDraw draw){
    nativeTextBegin = begin;
    nativeTextDraw = draw;
}

// 使用标准内存分配
#define GVIEW_MALLOC(size) malloc(size)
// #define VIEW_CALLOC(num, size) calloc(num, size)

// 快速内联函数
#define INLINE static inline __attribute__((always_inline))
#define FAST_CODE __attribute__((section(".iram1")))

//图片数据基础偏移
#define basicOffset 7

// 256色转RGB565查找表 - 修改为254个元素以匹配头文件声明
static const uint16_t COLOR256_16[254] = { 
    30751,24601,18451,12300,8201,39327,45887,52447,59007,20511,16409,12307,8204,6153,29087,39743,
    48351,56959,8223,6169,4115,2060,2057,18847,31551,42207,54911,31,25,19,12,9,12703,25407,40159,
    52863,415,313,243,140,105,13023,25631,40319,52927,799,633,467,300,233,13311,25855,40447,52991,
    1311,1049,787,524,393,13727,26175,40671,53119,1695,1337,1011,652,489,14047,26399,40831,53183,
    1887,1497,1139,748,553,14207,26527,40895,53215,2047,1657,1235,812,617,14335,26623,40959,53247,
    2036,1648,1228,808,614,14326,26616,40955,53245,2026,1640,1222,804,611,14318,26610,40951,53243,
    2016,1632,1216,800,608,14310,26604,40947,53241,12256,9824,7360,4896,2656,22502,32748,45043,
    55289,24544,18016,13504,8992,6752,32742,40940,49139,57337,38880,30304,21696,15136,10848,42982,
    49132,55283,61433,55264,42592,31936,21280,14944,57318,59372,61427,63481,65504,52832,40128,
    25376,19040,65510,65516,65523,65529,65280,52608,39968,25280,18944,65318,65388,65427,65497,
    64896,52320,39744,25120,18848,64998,65132,65267,65401,64640,52096,39584,25024,18752,64806,
    65004,65171,65369,64256,51808,39360,24864,18656,64486,64748,65011,65273,63872,51488,39136,
    24704,18528,64198,64524,64883,65209,63488,51200,38912,24576,18432,63878,64300,64723,65145,
    63504,51212,38921,24582,18436,63891,64310,64729,65148,63519,51225,38931,24588,18441,63903,
    64319,64735,65151,57375,45081,32787,22540,16393,57759,60223,62687,65151,33808,25388,19049,
    12678,8484,40147,46486,52857,59196,0,65535
};

uint16_t gviewPalette8Color(uint8_t index){
    return index < 254 ? COLOR256_16[index] : 0;
}

uint8_t fps = 20;
// 全局变量
void (*keyFun[10])(void) = {NULL};
uint16_t *picCache = NULL;
uint8_t *picCacheTemp = NULL;
uint32_t localeTime = 0;
uint16_t winW;
uint16_t winH;
volatile uint8_t spiSendHd = 0;//SPI发送标志
// 动态内存参数
// uint16_t mcDisplayObjectMax;
uint16_t moveClipMaxNum;
uint16_t displayObjMaxNum;
uint16_t MaxTweenNum;
uint16_t MaxActionNum;

// 全局对象
Stage stage = {NULL};
DisplayObject* displayMc = NULL;
TweenS* TweenArr = NULL;
DisObjAction* Action = NULL;

// 渲染区域
uint16_t rendSX, rendSY, rendEX, rendEY, rendW, rendH;

//按键
int keyState = 0;
uint8_t softKeyHeaderVisible = 0;
//其他
uint32_t FrameRate = 0;
uint32_t FrameRateTemp;
void (*enterFun)(void);
uint16_t bgColor = 0;
//数字类型对象缓存
char disObjftemp[20];


MemBlock *head = NULL;
uint16_t MemBlockAllNum = 0;

// ===================== 内联辅助函数 =====================
static inline uint16_t blend_colors_fast(uint16_t src,uint16_t dst,uint8_t alpha)
{
    if(alpha>=15)
        return src;

    if(alpha==0)
        return dst;


    uint8_t inv=15-alpha;


    uint32_t rb =
        ((src&0xF81F)*alpha+
         (dst&0xF81F)*inv)>>4;


    uint32_t g =
        ((src&0x07E0)*alpha+
         (dst&0x07E0)*inv)>>4;


    return
        (rb&0xF81F)|
        (g&0x07E0);
}
static inline uint16_t div15_fast(uint32_t x)
{
    return (x * 4369) >> 16;
}


static inline uint16_t blend_colors_fast_RGB454(
    uint16_t src,
    uint16_t dst,
    uint8_t alpha)
{
    // RGB454 A3 alpha转换
    if(alpha >= 7)
    {
        return src;
    }

    alpha <<= 1;

    if(alpha == 0)
    {
        return dst;
    }


    uint8_t inv =
        15 - alpha;



    uint16_t sr =
        src >> 11;

    uint16_t sg =
        (src >> 5) & 0x3F;

    uint16_t sb =
        src & 0x1F;



    uint16_t dr =
        dst >> 11;

    uint16_t dg =
        (dst >> 5) & 0x3F;

    uint16_t db =
        dst & 0x1F;



    uint16_t r =
        div15_fast(
            sr * alpha +
            dr * inv);



    uint16_t g =
        div15_fast(
            sg * alpha +
            dg * inv);



    uint16_t b =
        div15_fast(
            sb * alpha +
            db * inv);



    return
        (r << 11) |
        (g << 5) |
        b;
}
INLINE void set_pixel_fast(uint16_t x, uint16_t y, uint16_t color, uint8_t alpha)
{
    if (alpha == 0) return;
    
    size_t idx = (size_t)(x - rendSX) + (size_t)(y - rendSY) * rendW;
    uint16_t *pixel = &picCache[idx];
    
    if (alpha >= 15) {
        *pixel = (color >> 8) | (color << 8);
        return;
    }
    
    uint16_t current = (*pixel >> 8) | (*pixel << 8);
    if (current == color) return;
    
    uint16_t blended = blend_colors_fast(color, current, alpha);
    *pixel = (blended >> 8) | (blended << 8);
}

INLINE ClipRect calculate_clip_rect(int16_t x,int16_t y,uint16_t w,uint16_t h,uint8_t mcNum)
{
    ClipRect clip;


    //--------------------------------
    // 当前刷新区域裁剪
    //--------------------------------

    clip.left =
        (x > rendSX) ?
        x :
        rendSX;

    clip.top =
        (y > rendSY) ?
        y :
        rendSY;


    int32_t objRight =
        (int32_t)x + w;

    int32_t objBottom =
        (int32_t)y + h;


    clip.right =
        (objRight < rendEX) ?
        objRight :
        rendEX;

    clip.bottom =
        (objBottom < rendEY) ?
        objBottom :
        rendEY;



    //--------------------------------
    // MovieClip Mask裁剪
    //--------------------------------

    MovieClip *mc =
        &stage.movieclipArr[mcNum];


    if(mc->Mask)
    {
        int16_t mask_x =
            mc->Mask->x;

        int16_t mask_y =
            mc->Mask->y;


        //--------------------------------
        // Mask跟随MovieClip
        //--------------------------------

        if(mc->MaskFollow)
        {
            mask_x += mc->x;
            mask_y += mc->y;
        }


        int32_t mask_r =
            (int32_t)mask_x +
            mc->Mask->width;

        int32_t mask_b =
            (int32_t)mask_y +
            mc->Mask->height;


        if(clip.left < mask_x)
            clip.left = mask_x;

        if(clip.top < mask_y)
            clip.top = mask_y;

        if(clip.right > mask_r)
            clip.right = mask_r;

        if(clip.bottom > mask_b)
            clip.bottom = mask_b;
    }



    //--------------------------------
    // 判断是否存在有效渲染区域
    //--------------------------------

    clip.valid =
        (
            clip.left < clip.right &&
            clip.top < clip.bottom
        );


    return clip;
}

// 透明度实现函数
INLINE void setPicCacheAlpha(uint16_t *cacheDot, uint16_t ra, uint16_t ga, uint16_t ba, 
                             uint16_t rb, uint16_t gb, uint16_t bb, uint8_t alpha)
{
    if (alpha == 0) return;
    
    if (alpha >= 15) {
        uint16_t rgb = (ra << 11) | (ga << 5) | ba;
        *cacheDot = rgb >> 8 | rgb << 8;
        return;
    }
    
    uint16_t r = (ra * alpha + rb * (15 - alpha)) >> 4;
    uint16_t g = (ga * alpha + gb * (15 - alpha)) >> 4;
    uint16_t b = (ba * alpha + bb * (15 - alpha)) >> 4;
    
    // 限制范围
    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (b > 31) b = 31;
    
    uint16_t rgb = (r << 11) | (g << 5) | b;
    *cacheDot = rgb >> 8 | rgb << 8;
}



char* getFont(uint8_t size,uint8_t type,uint8_t line){
    if(size==12){
		return &asc2_1206[type][line];
    }else if(size==16){
		return &asc2_1608[type][line];
    }else if(size==18){
        // Safe fallback when the native LCD text buffer is unavailable.
        return &asc2_1608[type][((uint16_t)line * 16) / 18];
	}else if(size==32){
		if(type == ':'-32){
			return &asc2_3216[10][line*2];
		}else
		if(type == ' '-32){
			return &asc2_3216[11][line*2];
		}else{
			return &asc2_3216[type -16][line*2];
		}
	}
	return 0;
}

//获取字体
uint16_t getFont12(uint8_t *s,uint8_t line)
{
	uint16_t k =*(uint8_t*)(s+1)-0xA1+(*(uint8_t*)s-0xB0)*94;
	if(k<6763){
		const uint8_t* resoult = (const uint8_t*)GBChar12;
		return resoult[k*24+line*2]<<8|resoult[k*24+line*2+1];
	}
	return 0;
}

uint16_t getFont16(uint8_t *s,uint8_t line)
{
    uint16_t k =*(uint8_t*)(s+1)-0xA1+(*(uint8_t*)s-0xB0)*94;
	if(k<6763){
        const uint8_t* resoult = GBChar16;
        return (resoult[k*32+line*2]<<8)|resoult[k*32+line*2+1];
	}
	return 0;
}
//获取数字位数
uint8_t getLen(uint16_t len){
    if (len<10){
        return 1;
    }else
    if (len<100){
        return 2;
    }else
    if (len<1000){
        return 3;
    }else
    if (len<1000)
    {
        return 4;
    }else
    {
        return 5;
    }
}
// ===================== 内存管理函数 =====================
void* my_malloc(size_t size) {
    void *ptr = GVIEW_MALLOC(size);
    if (ptr != NULL) {
        MemBlock *block = (MemBlock *)GVIEW_MALLOC(sizeof(MemBlock));
        if (block != NULL) {
            block->address = ptr;
            block->next = head;
            block->previous = NULL;
            if (head != NULL) {
                head->previous = block;
            }
            head = block;
            MemBlockAllNum++;
        } else {
            free(ptr);
            ptr = NULL;
        }
    }
    return ptr;
}

void my_free(void *ptr) {
    if (ptr == NULL) return;
    
    MemBlock *block = head;
    while (block != NULL) {
        if (block->address == ptr) {
            if (block->previous != NULL) {
                block->previous->next = block->next;
            } else {
                head = block->next;
            }
            
            if (block->next != NULL) {
                block->next->previous = block->previous;
            }
            
            free(block->address);
            free(block);
            MemBlockAllNum--;
            return;
        }
        block = block->next;
    }
}


void free_all() {
    while (head != NULL) {
        MemBlock *next = head->next;
        free(head->address);
        free(head);
        head = next;
        MemBlockAllNum--;
    }
}

//获取图像数据的长宽值
uint16_t getPicWidth(uint8_t* p,uint8_t frame){
    uint16_t ff = frame*basicOffset;
    if(!p[4 + ff]&&!p[5 + ff]){
        ff = (p[3 + ff]-1)*basicOffset;
    }
    return p[2 + ff]<<8|p[3+ff];
}
uint16_t getPicHeight(uint8_t* p,uint8_t frame){
    uint16_t ff = frame*basicOffset;
    if(!p[4 + ff]&&!p[5 + ff]){
        ff = (p[3 + ff]-1)*basicOffset;
    }
    return p[4 + ff]<<8|p[5+ff];
}
//整合MASK 统一用displayobject来计算，可选类型使用type标记，支持Rect和pic两种类型的mask
/*
*sx,sy:检测的行或列是否在mask范围
*McNumber:所在的Moviclip层级
*/
uint8_t checkMaskY(short sy,short McNumber){
    if(stage.movieclipArr[McNumber].Mask!=NULL){
        short MY = stage.movieclipArr[McNumber].Mask->y+(stage.movieclipArr[McNumber].MaskFollow?stage.movieclipArr[McNumber].y:0);
        short MH = stage.movieclipArr[McNumber].Mask->height;
        if(sy>=MY&&sy<MY+MH){
            return 1;
        }
        return 0;
    }
    return 1;
}

uint8_t checkMaskX(short rx,short McNumber){
    if(stage.movieclipArr[McNumber].Mask!=NULL){
        short MX = stage.movieclipArr[McNumber].Mask->x+(stage.movieclipArr[McNumber].MaskFollow?stage.movieclipArr[McNumber].x:0);
        short MW = stage.movieclipArr[McNumber].Mask->width;

        if(rx>=MX&&rx<MX+MW){
            return 1;
        }
        return 0;
    }
    return 1;
}

// ===================== 优化渲染函数 =====================
static FAST_CODE void render_16bit_image(DisplayObject* disObj, int16_t mcx, int16_t mcy, uint8_t mcNum)
{
    if (!disObj->visible || disObj->alpha == 0) return;
    
    ClipRect clip = calculate_clip_rect(mcx + disObj->x, mcy + disObj->y, 
                                        disObj->width, disObj->height, mcNum);
    if (!clip.valid) return;
    
    uint16_t *img_data = (uint16_t *)(disObj->p + disObj->frameOffset);
    uint16_t img_width = disObj->width;
    uint32_t img_height = disObj->height;  // 改为uint32_t
    
    int16_t start_x = clip.left - (mcx + disObj->x);
    int16_t start_y = clip.top - (mcy + disObj->y);
    int16_t end_x = clip.right - (mcx + disObj->x);
    int16_t end_y = clip.bottom - (mcy + disObj->y);
    
    uint8_t alpha = disObj->alpha;
    uint16_t solid_color = disObj->color;
    uint8_t use_solid = (solid_color != COLORNONE);
    
    // 验证数据有效性
    if (!img_data || img_width == 0 || img_height == 0) return;
    
    for (int16_t ry = start_y; ry < end_y; ry++) {
        uint16_t y_pos = clip.top + (ry - start_y);
        uint32_t row_index = (uint32_t)ry * img_width;
        for (int16_t rx = start_x; rx < end_x; rx++) {
            uint32_t pixel_index = row_index + rx;
            
            // 检查索引是否在有效范围内
            if (pixel_index < (uint32_t)img_width * img_height) {
                uint16_t color = img_data[pixel_index];
                
                if (color != 0x0001) {  // 跳过透明色
                    if (use_solid) {
                        color = solid_color;
                    }
                    set_pixel_fast(clip.left + (rx - start_x), y_pos, color, alpha);
                }
            }
        }
    }
}

static FAST_CODE void render_8bit_image(DisplayObject* disObj, int16_t mcx, int16_t mcy, uint8_t mcNum)
{
    if (!disObj->visible || disObj->alpha == 0) return;
    
    ClipRect clip = calculate_clip_rect(mcx + disObj->x, mcy + disObj->y, 
                                        disObj->width, disObj->height, mcNum);
    if (!clip.valid) return;
    
    uint8_t *img_data = disObj->p + disObj->frameOffset;
    uint16_t img_width = disObj->width;
    
    int16_t start_x = clip.left - (mcx + disObj->x);
    int16_t start_y = clip.top - (mcy + disObj->y);
    int16_t end_x = clip.right - (mcx + disObj->x);
    int16_t end_y = clip.bottom - (mcy + disObj->y);
    
    uint8_t alpha = disObj->alpha;
    uint16_t solid_color = disObj->color;
    uint8_t use_solid = (solid_color != COLORNONE);
    
    for (int16_t ry = start_y; ry < end_y; ry++) {
        uint16_t y_pos = clip.top + (ry - start_y);
        uint8_t *line_src = &img_data[ry * img_width];
        for (int16_t rx = start_x; rx < end_x; rx++) {
            uint8_t color_idx = line_src[rx];
            
            if (color_idx != 0xFF) {
                uint16_t color = use_solid ? solid_color : COLOR256_16[color_idx];
                set_pixel_fast(clip.left + (rx - start_x), y_pos, color, alpha);
            }
        }
    }
}

static FAST_CODE void render_2bit_image(DisplayObject* disObj,int16_t mcx,int16_t mcy,uint8_t mcNum)
{
    if(!disObj->visible||disObj->alpha==0)
        return;


    ClipRect clip=calculate_clip_rect(
        mcx+disObj->x,
        mcy+disObj->y,
        disObj->width,
        disObj->height,
        mcNum);


    if(!clip.valid)
        return;



    uint8_t *pixel_data=
        disObj->p+
        disObj->frameOffset+
        6;


    uint16_t img_width=
        disObj->width;


    uint8_t alpha=
        disObj->alpha;


    uint16_t solid=
        disObj->color;


    uint8_t use_solid=
        (solid!=COLORNONE);


    uint8_t flip_x=
        (disObj->dir&0x0F)!=0;



    int16_t src_x=
        clip.left-(mcx+disObj->x);


    int16_t src_y=
        clip.top-(mcy+disObj->y);



    int16_t w=
        clip.right-clip.left;


    int16_t h=
        clip.bottom-clip.top;



    uint16_t palette_rgb[4]={0};
    uint16_t palette_fb[4]={0};



    if(!use_solid&&disObj->f)
    {
        uint8_t *p=(uint8_t*)disObj->f;


        for(int i=0;i<4;i++)
        {
            palette_rgb[i]=
                ((uint16_t)p[i*2]<<8)|
                p[i*2+1];


            palette_fb[i]=
                (palette_rgb[i]>>8)|
                (palette_rgb[i]<<8);
        }
    }



    uint16_t solid_fb=
        (solid>>8)|
        (solid<<8);



    //------------------------------------------------
    // 翻转模式
    //------------------------------------------------

    if(flip_x)
    {

        for(int y=0;y<h;y++)
        {

            uint16_t *fb=
                &picCache[
                (clip.top+y-rendSY)*rendW+
                clip.left-rendSX];


            uint32_t row=
                (uint32_t)(src_y+y)*img_width;



            for(int x=0;x<w;x++)
            {

                uint32_t pos=
                    row+
                    src_x+
                    (w-1-x);



                uint8_t id=
                    (pixel_data[pos>>2]>>((pos&3)*2))
                    &0x03;



                if(id==3)
                    continue;



                if(alpha>=15)
                {
                    fb[x]=use_solid?
                        solid_fb:
                        palette_fb[id];
                }
                else
                {

                    uint16_t color=
                        use_solid?
                        solid:
                        palette_rgb[id];


                    uint16_t cur=
                        (fb[x]>>8)|
                        (fb[x]<<8);



                    uint16_t out=
                        blend_colors_fast(
                            color,
                            cur,
                            alpha);


                    fb[x]=
                        (out>>8)|
                        (out<<8);
                }
            }
        }


        return;
    }



    //------------------------------------------------
    // 普通高速模式
    //------------------------------------------------

    if(alpha>=15)
    {

        for(int y=0;y<h;y++)
        {

            uint16_t *fb=
                &picCache[
                (clip.top+y-rendSY)*rendW+
                clip.left-rendSX];


            uint32_t index=
                (uint32_t)(src_y+y)*img_width+
                src_x;



            int x=0;



            //处理前面非4像素对齐
            while(x<w && (index&3))
            {

                uint8_t id=
                    (pixel_data[index>>2]>>((index&3)*2))
                    &3;



                if(id!=3)
                {
                    fb[x]=use_solid?
                        solid_fb:
                        palette_fb[id];
                }


                x++;
                index++;
            }



            //4像素展开
            while(x+4<=w)
            {

                uint8_t d=
                    pixel_data[index>>2];



                if(d==0xff)
                {
                    x+=4;
                    index+=4;
                    continue;
                }



                if(d==0x00)
                {
                    uint16_t c=
                        use_solid?
                        solid_fb:
                        palette_fb[0];


                    fb[x]=c;
                    fb[x+1]=c;
                    fb[x+2]=c;
                    fb[x+3]=c;


                    x+=4;
                    index+=4;
                    continue;
                }



                if(d==0x55)
                {
                    uint16_t c=
                        use_solid?
                        solid_fb:
                        palette_fb[1];


                    fb[x]=c;
                    fb[x+1]=c;
                    fb[x+2]=c;
                    fb[x+3]=c;


                    x+=4;
                    index+=4;
                    continue;
                }



                if(d==0xaa)
                {
                    uint16_t c=
                        use_solid?
                        solid_fb:
                        palette_fb[2];


                    fb[x]=c;
                    fb[x+1]=c;
                    fb[x+2]=c;
                    fb[x+3]=c;


                    x+=4;
                    index+=4;
                    continue;
                }



                uint8_t p0=d&3;
                uint8_t p1=(d>>2)&3;
                uint8_t p2=(d>>4)&3;
                uint8_t p3=d>>6;



                if(use_solid)
                {
                    if(p0!=3)fb[x]=solid_fb;
                    if(p1!=3)fb[x+1]=solid_fb;
                    if(p2!=3)fb[x+2]=solid_fb;
                    if(p3!=3)fb[x+3]=solid_fb;
                }
                else
                {
                    if(p0!=3)fb[x]=palette_fb[p0];
                    if(p1!=3)fb[x+1]=palette_fb[p1];
                    if(p2!=3)fb[x+2]=palette_fb[p2];
                    if(p3!=3)fb[x+3]=palette_fb[p3];
                }


                x+=4;
                index+=4;
            }



            //尾部
            while(x<w)
            {

                uint8_t id=
                    (pixel_data[index>>2]>>((index&3)*2))
                    &3;



                if(id!=3)
                {
                    fb[x]=use_solid?
                        solid_fb:
                        palette_fb[id];
                }


                x++;
                index++;
            }
        }


        return;
    }



    //------------------------------------------------
    // alpha普通模式
    //------------------------------------------------

    for(int y=0;y<h;y++)
    {

        uint16_t *fb=
            &picCache[
            (clip.top+y-rendSY)*rendW+
            clip.left-rendSX];


        uint32_t index=
            (uint32_t)(src_y+y)*img_width+
            src_x;



        for(int x=0;x<w;x++,index++)
        {

            uint8_t id=
                (pixel_data[index>>2]>>((index&3)*2))
                &3;



            if(id==3)
                continue;



            uint16_t color=
                use_solid?
                solid:
                palette_rgb[id];



            uint16_t cur=
                (fb[x]>>8)|
                (fb[x]<<8);



            if(cur!=color)
            {

                uint16_t out=
                    blend_colors_fast(
                        color,
                        cur,
                        alpha);


                fb[x]=
                    (out>>8)|
                    (out<<8);
            }
        }
    }
}

// Home forest sprites have no animation or tint. Decode them once, then copy
// opaque horizontal spans instead of unpacking every 4-bit pixel each frame.
typedef struct {
    uint16_t x, length;
} Fast4Span;

typedef struct {
    const uint8_t *source;
    uint16_t width, height;
    uint16_t *pixels;
    uint32_t *row_start;
    Fast4Span *spans;
} Fast4Cache;

#define FAST4_CACHE_SLOTS 4
static Fast4Cache fast4_caches[FAST4_CACHE_SLOTS];
static uint8_t fast4_cache_count;

static inline uint8_t fast4_index(const uint8_t *pixels, uint32_t index){
    return (pixels[index >> 1] >> ((index & 1) * 4)) & 15;
}

void gviewPrepareFast4BitSprite(const DisplayObject *sprite){
    if(!sprite || !sprite->p || sprite->picType != 4) return;
    for(uint8_t i=0; i<fast4_cache_count; ++i)
        if(fast4_caches[i].source == sprite->p) return;
    if(fast4_cache_count >= FAST4_CACHE_SLOTS) return;

    const uint16_t width = sprite->width, height = sprite->height;
    const uint32_t pixel_count = (uint32_t)width * height;
    const uint8_t *palette = sprite->p + sprite->frameOffset;
    const uint8_t *packed = palette + 30;
    uint32_t span_count = 0;
    for(uint16_t y=0; y<height; ++y){
        uint16_t x=0;
        while(x<width){
            if(fast4_index(packed, (uint32_t)y*width+x) == 15){++x; continue;}
            ++span_count;
            do {++x;} while(x<width && fast4_index(packed, (uint32_t)y*width+x) != 15);
        }
    }

    uint16_t *decoded = malloc((size_t)pixel_count * sizeof(uint16_t));
    uint32_t *row_start = malloc((size_t)(height+1) * sizeof(uint32_t));
    Fast4Span *spans = malloc((size_t)(span_count ? span_count : 1) * sizeof(Fast4Span));
    if(!decoded || !row_start || !spans){
        free(decoded); free(row_start); free(spans);
        return; // The normal 4-bit renderer remains available.
    }
    uint16_t colors[15];
    for(uint8_t i=0; i<15; ++i){
        const uint16_t rgb = ((uint16_t)palette[i*2] << 8) | palette[i*2+1];
        colors[i] = (rgb >> 8) | (rgb << 8);
    }
    uint32_t span = 0;
    for(uint16_t y=0; y<height; ++y){
        row_start[y] = span;
        uint16_t x=0;
        while(x<width){
            const uint32_t base = (uint32_t)y*width;
            uint8_t id = fast4_index(packed, base+x);
            if(id == 15){++x; continue;}
            const uint16_t start = x;
            do {
                decoded[base+x] = colors[id];
                ++x;
                if(x == width) break;
                id = fast4_index(packed, base+x);
            } while(id != 15);
            spans[span++] = (Fast4Span){start, (uint16_t)(x-start)};
        }
    }
    row_start[height] = span;
    fast4_caches[fast4_cache_count++] = (Fast4Cache){
        sprite->p, width, height, decoded, row_start, spans
    };
}

static FAST_CODE uint8_t render_fast4_image(DisplayObject *sprite, int16_t mcx,
                                              int16_t mcy, uint8_t mcNum){
    if(sprite->alpha != 15 || sprite->color != COLORNONE || (sprite->dir & 0x0f))
        return 0;
    const Fast4Cache *cache = NULL;
    for(uint8_t i=0; i<fast4_cache_count; ++i){
        if(fast4_caches[i].source == sprite->p){cache = &fast4_caches[i]; break;}
    }
    if(!cache || cache->width != sprite->width || cache->height != sprite->height)
        return 0;
    const int16_t origin_x = mcx + sprite->x;
    const int16_t origin_y = mcy + sprite->y;
    const ClipRect clip = calculate_clip_rect(origin_x, origin_y,
                                               sprite->width, sprite->height, mcNum);
    if(!clip.valid) return 1;
    const int16_t src_left = clip.left - origin_x;
    const int16_t src_right = clip.right - origin_x;
    for(int16_t y=clip.top; y<clip.bottom; ++y){
        const uint16_t src_y = y - origin_y;
        uint16_t *dst = &picCache[(y-rendSY)*rendW + clip.left-rendSX];
        const uint16_t *src = cache->pixels + (uint32_t)src_y*cache->width;
        for(uint32_t i=cache->row_start[src_y]; i<cache->row_start[src_y+1]; ++i){
            const Fast4Span run = cache->spans[i];
            const int16_t left = run.x > src_left ? run.x : src_left;
            const int16_t right = run.x + run.length < src_right ?
                                  run.x + run.length : src_right;
            if(right > left)
                memcpy(dst + left-src_left, src + left, (size_t)(right-left)*2);
        }
    }
    return 1;
}

static FAST_CODE void render_4bit_image(DisplayObject* disObj,int16_t mcx,int16_t mcy,uint8_t mcNum)
{
    if(!disObj->visible||disObj->alpha==0)
        return;
    if(render_fast4_image(disObj, mcx, mcy, mcNum)) return;


    ClipRect clip=calculate_clip_rect(
        mcx+disObj->x,
        mcy+disObj->y,
        disObj->width,
        disObj->height,
        mcNum);


    if(!clip.valid)
        return;


    uint8_t *pixel_data=
        disObj->p+
        disObj->frameOffset+
        30;


    uint16_t img_width=
        disObj->width;


    uint8_t alpha=
        disObj->alpha;


    uint16_t solid=
        disObj->color;


    uint8_t use_solid=
        (solid!=COLORNONE);


    uint8_t flip_x=
        (disObj->dir&0x0F)!=0;



    int16_t src_x=
        clip.left-(mcx+disObj->x);


    int16_t src_y=
        clip.top-(mcy+disObj->y);



    int16_t w=
        clip.right-clip.left;


    int16_t h=
        clip.bottom-clip.top;



    uint16_t palette_rgb[16]={0};
    uint16_t palette_fb[16]={0};



    if(!use_solid&&disObj->f)
    {
        uint8_t *p=(uint8_t*)disObj->f;

        for(int i=0;i<15;i++)
        {
            palette_rgb[i]=
                ((uint16_t)p[i*2]<<8)|
                p[i*2+1];


            palette_fb[i]=
                (palette_rgb[i]>>8)|
                (palette_rgb[i]<<8);
        }
    }



    uint16_t solid_fb=
        (solid>>8)|
        (solid<<8);



    //------------------------------------------------
    // 翻转模式
    //------------------------------------------------

    if(flip_x)
    {

        for(int y=0;y<h;y++)
        {

            uint16_t *fb=
                &picCache[
                (clip.top+y-rendSY)*rendW+
                clip.left-rendSX];


            uint32_t row=
                (uint32_t)(src_y+y)*img_width;



            for(int x=0;x<w;x++)
            {

                uint32_t pos=
                    row+
                    src_x+
                    (w-1-x);



                uint8_t id=
                    (pixel_data[pos>>1]>>((pos&1)*4))
                    &0x0F;



                if(id==15)
                    continue;



                if(alpha>=15)
                {
                    fb[x]=use_solid?
                        solid_fb:
                        palette_fb[id];
                }
                else
                {

                    uint16_t color=
                        use_solid?
                        solid:
                        palette_rgb[id];


                    uint16_t cur=
                        (fb[x]>>8)|
                        (fb[x]<<8);



                    uint16_t out=
                        blend_colors_fast(
                            color,
                            cur,
                            alpha);


                    fb[x]=
                        (out>>8)|
                        (out<<8);
                }
            }
        }


        return;
    }



    //------------------------------------------------
    // 普通高速模式
    //------------------------------------------------

    if(alpha>=15)
    {

        for(int y=0;y<h;y++)
        {

            uint16_t *fb=
                &picCache[
                (clip.top+y-rendSY)*rendW+
                clip.left-rendSX];


            uint32_t index=
                (uint32_t)(src_y+y)*img_width+
                src_x;



            int x=0;



            //处理非2像素对齐
            while(x<w && (index&1))
            {

                uint8_t id=
                    (pixel_data[index>>1]>>((index&1)*4))
                    &0x0F;



                if(id!=15)
                {
                    fb[x]=use_solid?
                        solid_fb:
                        palette_fb[id];
                }


                x++;
                index++;
            }



            //2像素展开
            while(x+2<=w)
            {

                uint8_t d=
                    pixel_data[index>>1];


                uint8_t p0=
                    d&0x0F;


                uint8_t p1=
                    d>>4;



                if(use_solid)
                {

                    if(p0!=15)
                        fb[x]=solid_fb;


                    if(p1!=15)
                        fb[x+1]=solid_fb;

                }
                else
                {

                    if(p0!=15)
                        fb[x]=palette_fb[p0];


                    if(p1!=15)
                        fb[x+1]=palette_fb[p1];

                }


                x+=2;
                index+=2;
            }



            //尾部
            while(x<w)
            {

                uint8_t id=
                    (pixel_data[index>>1]>>((index&1)*4))
                    &0x0F;



                if(id!=15)
                {
                    fb[x]=use_solid?
                        solid_fb:
                        palette_fb[id];
                }


                x++;
                index++;
            }
        }


        return;
    }



    //------------------------------------------------
    // alpha普通模式
    //------------------------------------------------

    for(int y=0;y<h;y++)
    {

        uint16_t *fb=
            &picCache[
            (clip.top+y-rendSY)*rendW+
            clip.left-rendSX];


        uint32_t index=
            (uint32_t)(src_y+y)*img_width+
            src_x;



        for(int x=0;x<w;x++,index++)
        {

            uint8_t id=
                (pixel_data[index>>1]>>((index&1)*4))
                &0x0F;



            if(id==15)
                continue;



            uint16_t color=
                use_solid?
                solid:
                palette_rgb[id];



            uint16_t cur=
                (fb[x]>>8)|
                (fb[x]<<8);



            if(cur!=color)
            {

                uint16_t out=
                    blend_colors_fast(
                        color,
                        cur,
                        alpha);


                fb[x]=
                    (out>>8)|
                    (out<<8);
            }
        }
    }
}



static FAST_CODE void render_1bit_image(DisplayObject* disObj,int16_t mcx,int16_t mcy,uint8_t mcNum)
{
    if(!disObj->visible||disObj->alpha==0)
        return;


    ClipRect clip=calculate_clip_rect(
        mcx+disObj->x,
        mcy+disObj->y,
        disObj->width,
        disObj->height,
        mcNum);


    if(!clip.valid)
        return;


    uint8_t *img_data=
        disObj->p+
        disObj->frameOffset;


    uint16_t img_width=
        getPicWidth(disObj->p,0);


    uint16_t color=
        disObj->color;


    uint8_t alpha=
        disObj->alpha;


    uint8_t flip_x=
        (disObj->dir&0x0F)!=0;



    int16_t src_x=
        clip.left-(mcx+disObj->x);


    int16_t src_y=
        clip.top-(mcy+disObj->y);



    int16_t w=
        clip.right-clip.left;


    int16_t h=
        clip.bottom-clip.top;



    uint16_t color_fb=
        (color>>8)|
        (color<<8);



    //------------------------------------------------
    // 翻转模式
    //------------------------------------------------

    if(flip_x)
    {

        for(int y=0;y<h;y++)
        {

            uint16_t *fb=
                &picCache[
                (clip.top+y-rendSY)*rendW+
                clip.left-rendSX];


            uint32_t row=
                (uint32_t)(src_y+y)*img_width;



            for(int x=0;x<w;x++)
            {

                uint32_t pos=
                    row+
                    src_x+
                    (w-1-x);



                uint8_t bit=
                    (img_data[pos>>3]>>(pos&7))&1;



                if(bit)
                {

                    if(alpha>=15)
                    {
                        fb[x]=color_fb;
                    }
                    else
                    {

                        uint16_t cur=
                            (fb[x]>>8)|
                            (fb[x]<<8);


                        uint16_t out=
                            blend_colors_fast(
                                color,
                                cur,
                                alpha);


                        fb[x]=
                            (out>>8)|
                            (out<<8);
                    }
                }
            }
        }


        return;
    }




    //------------------------------------------------
    // 普通高速模式
    //------------------------------------------------

    if(alpha>=15)
    {

        for(int y=0;y<h;y++)
        {

            uint16_t *fb=
                &picCache[
                (clip.top+y-rendSY)*rendW+
                clip.left-rendSX];


            uint32_t index=
                (uint32_t)(src_y+y)*img_width+
                src_x;


            int x=0;



            while(x<w && (index&7))
            {

                uint8_t bit=
                    (img_data[index>>3]>>(index&7))&1;


                if(bit)
                    fb[x]=color_fb;


                x++;
                index++;
            }



            while(x+8<=w)
            {

                uint8_t d=
                    img_data[index>>3];


                if(d==0x00)
                {
                    x+=8;
                    index+=8;
                    continue;
                }


                if(d==0xff)
                {
                    fb[x]=color_fb;
                    fb[x+1]=color_fb;
                    fb[x+2]=color_fb;
                    fb[x+3]=color_fb;
                    fb[x+4]=color_fb;
                    fb[x+5]=color_fb;
                    fb[x+6]=color_fb;
                    fb[x+7]=color_fb;

                    x+=8;
                    index+=8;
                    continue;
                }


                if(d&0x01)fb[x]=color_fb;
                if(d&0x02)fb[x+1]=color_fb;
                if(d&0x04)fb[x+2]=color_fb;
                if(d&0x08)fb[x+3]=color_fb;
                if(d&0x10)fb[x+4]=color_fb;
                if(d&0x20)fb[x+5]=color_fb;
                if(d&0x40)fb[x+6]=color_fb;
                if(d&0x80)fb[x+7]=color_fb;


                x+=8;
                index+=8;
            }



            while(x<w)
            {

                uint8_t bit=
                    (img_data[index>>3]>>(index&7))&1;


                if(bit)
                    fb[x]=color_fb;


                x++;
                index++;
            }
        }


        return;
    }



    //------------------------------------------------
    // alpha普通模式
    //------------------------------------------------

    for(int y=0;y<h;y++)
    {

        uint16_t *fb=
            &picCache[
            (clip.top+y-rendSY)*rendW+
            clip.left-rendSX];


        uint32_t index=
            (uint32_t)(src_y+y)*img_width+
            src_x;



        for(int x=0;x<w;x++,index++)
        {

            uint8_t bit=
                (img_data[index>>3]>>(index&7))&1;


            if(!bit)
                continue;



            uint16_t cur=
                (fb[x]>>8)|
                (fb[x]<<8);



            uint16_t out=
                blend_colors_fast(
                    color,
                    cur,
                    alpha);



            fb[x]=
                (out>>8)|
                (out<<8);
        }
    }
}

static const uint8_t rgb454_alpha_lut[8]=
{
    0,
    1,
    2,
    3,
    5,
    8,
    11,
    15
};

static FAST_CODE void render_rgb454_image(DisplayObject* disObj,int16_t mcx,int16_t mcy,uint8_t mcNum)
{
    if(!disObj->visible||disObj->alpha==0)
        return;


    ClipRect clip=calculate_clip_rect(
        mcx+disObj->x,
        mcy+disObj->y,
        disObj->width,
        disObj->height,
        mcNum);


    if(!clip.valid)
        return;



    uint16_t *img_data=
        (uint16_t*)(disObj->p+disObj->frameOffset);


    uint16_t img_width=
        disObj->width;


    uint8_t obj_alpha=
        disObj->alpha;


    uint16_t solid_color=
        disObj->color;


    uint8_t use_solid=
        (solid_color!=COLORNONE);


    uint8_t flip_x=
        (disObj->dir&0x0F)!=0;



    int16_t src_x=
        clip.left-(mcx+disObj->x);


    int16_t src_y=
        clip.top-(mcy+disObj->y);


    int16_t w=
        clip.right-clip.left;


    int16_t h=
        clip.bottom-clip.top;



#define RGB454_TO_RGB565(c,out)       \
{                                     \
    uint16_t r=((c&0x1E00)>>9);       \
    uint16_t g=((c&0x01F0)>>4);       \
    uint16_t b=(c&0x000F);            \
    r=(r<<1)|(r>>3);                  \
    g=(g<<1)|(g>>4);                  \
    b=(b<<1)|(b>>3);                  \
    out=(r<<11)|(g<<5)|b;             \
}



    for(int y=0;y<h;y++)
    {

        uint16_t *fb=
            &picCache[
            (clip.top+y-rendSY)*rendW+
            clip.left-rendSX];



        uint16_t *src;



        if(flip_x)
        {
            src=
            &img_data[
            (src_y+y)*img_width+
            src_x+w-1];
        }
        else
        {
            src=
            &img_data[
            (src_y+y)*img_width+
            src_x];
        }



        for(int x=0;x<w;x++)
        {

            uint16_t raw=
                flip_x?
                *src--:
                src[x];



            if(raw==0x0001)
                continue;



            uint16_t color=
                (raw>>8)|
                (raw<<8);



            uint8_t a=
                color>>13;



            if(a==0)
                continue;



            uint16_t src_color;



            if(use_solid)
            {
                src_color=
                    solid_color;
            }
            else
            {
                RGB454_TO_RGB565(
                    color,
                    src_color);
            }



            // 图片自身alpha
            uint8_t final_alpha=
                rgb454_alpha_lut[a];



            // 叠加对象alpha
            if(obj_alpha<15)
            {
                final_alpha=
                    (final_alpha*obj_alpha)/15;
            }



            if(final_alpha>=15)
            {

                fb[x]=
                    (src_color>>8)|
                    (src_color<<8);

            }
            else
            {

                uint16_t dst=
                    (fb[x]>>8)|
                    (fb[x]<<8);



                uint16_t out=
                    blend_colors_fast_RGB454(
                        src_color,
                        dst,
                        final_alpha);



                fb[x]=
                    (out>>8)|
                    (out<<8);
            }
        }
    }
}

static FAST_CODE void render_rectangle(DisplayObject* disObj, int16_t mcx, int16_t mcy, uint8_t mcNum)
{
    if (!disObj->visible || disObj->alpha == 0) return;
    
    ClipRect clip = calculate_clip_rect(mcx + disObj->x, mcy + disObj->y, 
                                        disObj->width, disObj->height, mcNum);
    if (!clip.valid) return;
    
    uint16_t color = disObj->color;
    uint8_t alpha = disObj->alpha;
    
    if(alpha >= 15){
        //渲染第一行
        size_t idxC = (size_t)(clip.left - rendSX) + (size_t)(clip.top - rendSY) * rendW;
        uint16_t *pixelC = &picCache[idxC];
        for (uint16_t x = clip.left; x < clip.right; x++) {
            picCache[idxC + (x - clip.left)] = (color >> 8) | (color << 8);
        }
        for (uint16_t y = clip.top+1; y < clip.bottom; y++) {
            size_t idx = (size_t)(clip.left - rendSX) + (size_t)(y - rendSY) * rendW;
            uint16_t *pixel = &picCache[idx];
            memcpy(pixel, pixelC, (clip.right - clip.left) * 2);
        }
    }else
    for (uint16_t y = clip.top; y < clip.bottom; y++) {
        for (uint16_t x = clip.left; x < clip.right; x++) {
            set_pixel_fast(x, y, color, alpha);
        }
    }
}

// ===================== 原有函数（部分优化）=====================

// 申请新内存存储旋转后的图像数据并返回改内存指针
void refrushImgRotate(DisObjAction* action)  
{  
    float angle = action->angle  * 3.14 / 180.0f;  //旋转角度  
    float co = cos(angle);   //余弦  
    float si = sin(angle);   //正弦  
    int dstW,dstH;    //旋转后图像的高宽  
    int xOff,yOff;  //偏移  
    float xSrc;  
    float ySrc; //变换后图像的坐标在原图中的坐标  
  
    uint16_t srcW = getPicWidth(action->p,action->currentFrame);
    uint16_t srcH = getPicHeight(action->p,action->currentFrame);
    //计算旋转后的坐标范围  
    dstH = srcW*fabs(si)+srcH*fabs(co);  
    dstW = srcW*fabs(co)+srcH*fabs(si);  

    uint16_t picLength = 0;
    if(action->p[0] == 16){
        picLength = dstW*dstH*2*sizeof(unsigned char)+14;
    }else if(action->p[0] == 13){  // 新增
        picLength = dstW*dstH*2*sizeof(unsigned char)+14;
    }else if(action->p[0] == 8){
        picLength = dstW*dstH*sizeof(unsigned char)+14;
    }else if(action->p[0] == 4){
        picLength = dstW*dstH/2*sizeof(unsigned char)+14+30;
    }else if(action->p[0] == 2){
        picLength = dstW*dstH/4*sizeof(unsigned char)+14+6;
    }
    my_free(action->disObj->p);//释放原图内存
    unsigned char* pDst = (unsigned char*)my_malloc(picLength);  //用完需要释放
    memset(pDst,0xFF,picLength);  
    pDst[0] = action->p[0];//类型
    pDst[1] = 1;//帧数
    pDst[2] = dstW;//宽
    pDst[3] = dstH;//高
    pDst[4] = 0x7f;//偏移
    pDst[5] = 0x7f;//偏移
    pDst[6] = 1;//下一帧间隔（帧）
    
    action->disObj->p = pDst;    
    action->disObj->frameOffset = basicOffset + 2;
    //计算偏移  
    xOff=dstW/2;  
    yOff=dstH/2;  
  
    
    for (uint8_t y=0;y<dstH;y++)  
    {  
        for (uint8_t x=0;x<dstW;x++)  
        {  
            //求取在原图中的坐标  
            ySrc = si*(x-xOff)+co*(y-yOff)+(srcH/2);  
            xSrc = co*(x-xOff)-si*(y-yOff)+(srcW/2);  
            
            //如果在原图范围内  
            if (ySrc>=0&& ySrc<srcH-0.5f && xSrc>=0&& xSrc<srcW-0.5f)  
            {  
                if(action->p[0] == 16){
                    // 使用uint32_t防止溢出
                    uint32_t src_index = (uint32_t)ySrc * srcW + (uint32_t)xSrc;
                    uint32_t dst_index = (uint32_t)y * dstW + (uint32_t)x;
                    
                    if (src_index < (uint32_t)srcW * srcH && dst_index < (uint32_t)dstW * dstH) {
                        uint16_t color = ((uint16_t *)(action->p + action->frameOffset))[src_index];
                        ((uint16_t *)pDst)[dst_index] = color;
                    } else {
                        // 超出范围，设置为透明
                        ((uint16_t *)pDst)[dst_index] = 0x0001;
                    }
                } else if(action->p[0] == 13){
                    // RGB454+A3格式
                    uint16_t src_color = ((uint16_t *)(action->p + action->frameOffset))[(uint16_t)ySrc * srcW + (uint16_t)xSrc];
                    src_color = src_color >> 8 | src_color << 8;  // 字节交换
                    
                    uint16_t a = src_color >> 13;
                    if (a > 0) {
                        uint16_t ra = ((src_color & 0x1E00) >> 9) * 2;
                        uint16_t ga = ((src_color & 0x01F0) >> 4) * 2;
                        uint16_t ba = (src_color & 0x000F) * 2;
                        
                        uint16_t dst_color = (a << 13) | ((ra/2) << 9) | ((ga/2) << 4) | (ba/2);
                        dst_color = dst_color >> 8 | dst_color << 8;
                        
                        ((uint16_t *)pDst)[y * dstW + x] = dst_color;
                    } else {
                        // 完全透明
                        ((uint16_t *)pDst)[y * dstW + x] = 0x0001;
                    }
                }else if(action->p[0] == 8){
                    pDst[y*dstW+x+basicOffset+2] = action->p[(uint16_t)ySrc*srcW+(uint16_t)xSrc+action->frameOffset];
                }else if(action->p[0] == 4){
                    uint16_t pixNumByte = (uint16_t)ySrc*srcW+(uint16_t)xSrc;
                    uint16_t pixNumByteDst = y*dstW+x;
                    uint8_t pixNumBit = (pixNumByte%2)*4;//当前像素在字节中的位置
                    uint8_t pixNumBit_num = (action->p[action->frameOffset+30+pixNumByte/2]>>pixNumBit)&0x0F;
                    if(pixNumByteDst%2){
                        pDst[pixNumByteDst/2+basicOffset+2+30] = (pDst[pixNumByteDst/2+basicOffset+2+30]&0x0F)|(pixNumBit_num<<4);
                    }else{
                        pDst[pixNumByteDst/2+basicOffset+2+30] = (pDst[pixNumByteDst/2+basicOffset+2+30]&0xF0)|(pixNumBit_num);
                    }
                }else if(action->p[0] == 2){
                    // 2位色处理
                    uint32_t pixNumByte = (uint16_t)ySrc*srcW+(uint16_t)xSrc;
                    uint32_t pixNumByteDst = y*dstW+x;
                    uint8_t pixNumBit = (pixNumByte%4)*2;  // 每个像素2位
                    uint8_t pixNumBit_num = (action->p[action->frameOffset+6+pixNumByte/4]>>pixNumBit)&3;
                    
                    uint8_t byte_index = 6 + pixNumByteDst / 4;
                    uint8_t bit_shift = (pixNumByteDst % 4) * 2;
                    pDst[byte_index] = (pDst[byte_index] & ~(0x03 << bit_shift)) | (pixNumBit_num << bit_shift);
                }
            }else{
                if(action->p[0] == 16){
                    pDst[(y*dstW+x)*2+basicOffset+2+1] = 0x1;
                    pDst[(y*dstW+x)*2+basicOffset+2] = 0x0;
                }
            }
        }
    }
    action->disObj->width = getPicWidth(action->disObj->p,0);
    action->disObj->height = getPicHeight(action->disObj->p,0);
}  

//--------获取一个未被使用的Action------
DisObjAction* getAction(void){
    for(uint16_t i=0;i<MaxActionNum;i++){
        if(!Action[i].Type){
            return &Action[i];
        }
    }
    return &Action[0];
}

void resetAcFrameTime(DisObjAction *ac){
    //改变数据触发强制刷新
    ac->disObj->width = getPicWidth(ac->p,ac->currentFrame);
    ac->disObj->height = getPicHeight(ac->p,ac->currentFrame);

    ac->frameTime = ac->p[basicOffset+1+ac->currentFrame*basicOffset]*ac->frameMultiple;
    ac->frameOffset = getPicOffset(ac->currentFrame,ac->p);//动画帧偏移
    ac->disObj->frameOffset = ac->frameOffset;
    if(ac->disObj->type != 0xF4)//固定色值
    ac->disObj->f = (char*)(ac->disObj->p + ac->frameOffset);//帧率;//强制刷新图像
}

void AcStop(DisObjAction* doAc){    
    doAc->playing = 0;
}
void AcPlay(DisObjAction* doAc){    
    doAc->playing = 1;
}
void AcGotoAndStop(DisObjAction* doAc,uint8_t frame){    
    if(frame > doAc->totallFrame-1)frame = doAc->totallFrame-1;
    doAc->currentFrame = frame;
    doAc->playing = 0;
    
    resetAcFrameTime(doAc);
}

void AcGotoAndPlay(DisObjAction* doAc,uint8_t frame){    
    doAc->currentFrame = (frame > doAc->totallFrame-1)?(doAc->totallFrame-1):frame;
    doAc->playing = 1;
    resetAcFrameTime(doAc);
}

void AcNextFrame(DisObjAction* doAc){  
    doAc->currentFrame = (doAc->currentFrame>=doAc->totallFrame-1)?0:doAc->currentFrame+1;
    resetAcFrameTime(doAc);
}

DisplayObject* getNewMvAction(const void *pic,short x,short y){
    DisObjAction *doAc = getAction(); 
    doAc->disObj = newPic(pic,x,y);
    doAc->disObj->ac = doAc;
    doAc->Type = 1;
    doAc->p = (uint8_t *)pic;
    doAc->width = doAc->disObj->width;
    doAc->height = doAc->disObj->height;
    doAc->angle = doAc->angleCurrent = 0;//旋转角度
    doAc->x = x;
    doAc->y = y;
    doAc->offsetx = 0;
    doAc->offsety = 0;
    //------------以下是动画-------------
    doAc->totallFrame = doAc->disObj->p[1]; //总帧数
    doAc->frameTime = doAc->disObj->p[8];//帧率
    doAc->frameMultiple = 1;
    doAc->currentFrame = 0;//当前帧数
    doAc->playing = 1;
    //--------缩放默认值----
    doAc->scaleXNow = 100;
    doAc->scaleX = 100;
    doAc->scaleYNow = 100;
    doAc->scaleY = 100;
    //---动画帧偏移---
    doAc->frameOffset = getPicOffset(0,pic);
    return doAc->disObj;
}

void setScalePic(DisObjAction* action){
    uint8_t *p = action->p;
    uint16_t srcW = getPicWidth(p,action->currentFrame);
    uint16_t srcH = getPicHeight(p,action->currentFrame);
    uint16_t dstW = (uint16_t)(srcW*action->scaleX/100);
    uint16_t dstH = (uint16_t)(srcH*action->scaleY/100);
    size_t picLength = 0;
    if(p[0] == 16||p[0] == 13){
        picLength = dstW*dstH*2*sizeof(unsigned char)+basicOffset+2+2;
    }else if(p[0] == 8){
        picLength = dstW*dstH*sizeof(unsigned char)+basicOffset+2+2;
    }else if(p[0] == 4){
        picLength = dstW*dstH/2*sizeof(unsigned char)+basicOffset+2+32;
    }else if(p[0] == 2){
        picLength = dstW*dstH/4*sizeof(unsigned char)+basicOffset+2+8;
    }else if(p[0] == 1){
        picLength = gview_1bit_bytes(dstW, dstH) + basicOffset + 2;
    }
    uint8_t *pDst = (uint8_t*)my_malloc(picLength);
    if (!pDst) {
        action->disObj->p = p;
        action->disObj->width = srcW;
        action->disObj->height = srcH;
        action->disObj->frameOffset = action->frameOffset;
        action->disObj->f = (char *)(p + action->frameOffset);
        action->scaleX = action->scaleY = 100;
        action->scaleXNow = action->scaleYNow = 100;
        return;
    }
    memset(pDst, 0, picLength);  //用完需要释放
    pDst[0] = p[0];
    pDst[1] = 1;
    pDst[2] = dstW>>8&0xFF;
    pDst[3] = dstW&0xFF;
    pDst[4] = dstH>>8&0xFF;
    pDst[5] = dstH&0xFF;
    pDst[6] = 0x7f;
    pDst[7] = 0x7f;
    pDst[8] = 1;

    float snumx = 100.0f/action->scaleX;//缩放倍数转化
    float snumy = 100.0f/action->scaleY;//缩放倍数转化
    uint16_t x,y;
    uint16_t ytemp1,ytemp2;
    uint16_t pixOffset = action->frameOffset;//2+p[1]*5;
    action->disObj->p = pDst;
    if(p[0] == 16){
        for(y = 0; y < dstH; y++){
            uint32_t src_y = (uint32_t)(y * snumy);
            if (src_y >= srcH) continue;
            
            for(x = 0; x < dstW; x++){
                uint32_t src_x = (uint32_t)(x * snumx);
                if (src_x >= srcW) continue;
                
                uint32_t src_index = src_y * srcW + src_x;
                if (src_index < (uint32_t)srcW * srcH) {
                    uint16_t color = ((uint16_t *)(p + pixOffset))[src_index];
                    ((uint16_t *)pDst)[basicOffset + 2 + y * dstW + x] = color;
                } else {
                    ((uint16_t *)pDst)[basicOffset + 2 + y * dstW + x] = 0x0001;
                }
            }
        }
    }else if(p[0] == 13){

        uint16_t *src_data = (uint16_t *)(p + pixOffset);
        uint16_t *dst_data = (uint16_t *)(pDst + basicOffset +2);
        
        for(uint16_t y = 0; y < dstH; y++){
            uint32_t src_y = (uint32_t)(y * snumy);
            for(uint16_t x = 0; x < dstW; x++){
                uint32_t src_x = (uint32_t)(x * snumx);
                
                if (src_y < srcH && src_x < srcW) {
                    uint16_t src_color = src_data[src_y * srcW + src_x];
                    dst_data[y * dstW + x] = src_color;
                } else {
                    dst_data[y * dstW + x] = 0x0001;  // 透明像素
                }
            }
        }
    }else if(p[0] == 8){
        for(y = 0;y<dstH;y++){
            ytemp1 = y * dstW + pixOffset;//目标填充
            ytemp2 = (uint16_t)(y*snumy)*srcW + pixOffset;
            for(x = 0;x<dstW;x++){                
                pDst[x+ytemp1] = p[ytemp2+(uint16_t)(x*snumx)];
            }
        }
    }else if(p[0] == 4){
        // 复制调色板（30字节）
        memcpy(pDst + basicOffset +2, p + pixOffset, 30);
        action->disObj->f = (char*)(action->disObj->p + basicOffset +2);//帧率;//强制刷新图像
        for(y = 0;y<dstH;y++){
            for(x = 0;x<dstW;x++){
                uint16_t pixNumByte = (uint16_t)(y==dstH-1?srcH-1:y*snumy)*srcW+(uint16_t)(x==dstW-1?srcW-1:x*snumx);//当前像素在原图像中的位置
                uint16_t pixNumByteDst = y*dstW+x;//当前像素在新图像中的位置
                uint8_t pixNumBit = (pixNumByte%2)*4;//当前像素在字节中的位置
                uint8_t pixNumBit_num = (p[pixOffset+30+pixNumByte/2]>>pixNumBit)&0x0F;
                if(pixNumByteDst%2){
                    pDst[pixNumByteDst/2+basicOffset+2+30] = (pDst[pixNumByteDst/2+basicOffset+2+30]&0x0F)|(pixNumBit_num<<4);
                }else{
                    pDst[pixNumByteDst/2+basicOffset+2+30] = (pDst[pixNumByteDst/2+basicOffset+2+30]&0xF0)|(pixNumBit_num);
                }
            }
        }
    }else if(p[0] == 2){
        // 2位色
        // 复制调色板（6字节）
        memcpy(pDst + basicOffset +2, p + pixOffset, 6);
        action->disObj->f = (char*)(action->disObj->p + basicOffset +2);//帧率;//强制刷新图像
        // 处理像素数据
        for(y = 0; y < dstH; y++){
            for(x = 0; x < dstW; x++){
                uint16_t pixNumByte = (uint16_t)(y==dstH-1?srcH-1:y*snumy)*srcW+(uint16_t)(x==dstW-1?srcW-1:x*snumx);//当前像素在原图像中的位置
                uint16_t pixNumByteDst = y*dstW+x;//当前像素在新图像中的位置
                uint16_t pixNumBit = (pixNumByte%4)*2;//当前像素在字节中的位置
                uint16_t pixNumBit_num = (p[pixOffset+6+pixNumByte/4]>>pixNumBit)&3;
                
                uint16_t byte_index = basicOffset + 2 + 6 + pixNumByteDst / 4;  // 7+6=13，像素数据从第13字节开始
                uint8_t bit_shift = (pixNumByteDst % 4) * 2;
                pDst[byte_index] = (pDst[byte_index] & ~(0x03 << bit_shift)) | (pixNumBit_num << bit_shift);
            }
        }
    }else if(p[0] == 1){
        // No image-sized temporary array on the render task's stack.
        gview_scale_1bit(p + pixOffset, srcW, srcH,
                         pDst + basicOffset + 2, dstW, dstH);
    }

    action->disObj->p = pDst;
    action->disObj->width = dstW;
    action->disObj->height = dstH;  
    action->disObj->frameOffset = basicOffset + 2;
    
    //计算缩放后的偏移值,让图像居中缩放
    action->offsetx = (dstW - srcW)/2;
    action->offsety = (dstH - srcH)/2;

    action->scaleXNow = action->scaleX;
    action->scaleYNow = action->scaleY;
}

void refrushScaleAction(DisObjAction* action){
    if (action->disObj->p != action->p) {
        my_free(action->disObj->p);
        action->disObj->p = action->p;
    }
    if(action->scaleX == 100&&action->scaleY == 100){
        action->disObj->p = action->p;   
        action->offsetx = 0;
        action->offsety = 0;
        action->disObj->width = getPicWidth(action->p,action->currentFrame);
        action->disObj->height = getPicHeight(action->p,action->currentFrame);
        action->disObj->frameOffset = action->frameOffset;
        if(action->disObj->type != 0xF4)//如果是固定色值
        action->disObj->f = (char*)(action->disObj->p + action->frameOffset);//帧率;//强制刷新图像
    }else{
        setScalePic(action); 
    }
}

void refrushRotaAction(DisObjAction* action){
    if(action->disObj->p!=action->p)my_free(action->disObj->p);            
    if(action->angle == 0){
        action->disObj->p = action->p;   
        action->offsetx = 0;
        action->offsety = 0;
        action->disObj->width = getPicWidth(action->p,action->currentFrame);
        action->disObj->height = getPicHeight(action->p,action->currentFrame);
        action->disObj->frameOffset = action->frameOffset;
        if(action->disObj->type != 0xF4)//固定色值//如果是固定色值
        action->disObj->f = (char*)(action->disObj->p + action->frameOffset);//帧率;//强制刷新图像

    }else{
        refrushImgRotate(action);
    }
}

void rotaAction(DisObjAction* action,short angle){
    if(action->angleCurrent!=angle){
        action->angle = angle;   
        refrushRotaAction(action);
        action->angleCurrent = action->angle;
    }
}

void freeAction(DisObjAction* action){
    if(action->Type){
        action->Type = 0;
        action->p = NULL;
        action->disObj = NULL;
    }
}

void deleteDisplayObject(DisplayObject *disObj){
    disObj->type = 0;
    disObj->visible = 1;
    disObj->alpha = 15;
    disObj->childrenNum = 0;
    disObj->ac = NULL;
    disObj->parent = NULL;
    disObj->deep = 0;
    disObj = NULL;
}


//添加显示对象到显示对象
void addChildToObjAt(DisplayObject *disObj,DisplayObject *child,uint8_t num){
    if(num>disObj->childrenNum)num = disObj->childrenNum;

    for(uint8_t i = disObj->childrenNum;i>num;i--){
        getChildAt(disObj,i-1)->deep += 1; 
    }
    child->parent = disObj;
    child->deep = num;
    disObj->childrenNum ++;
}
void addChildToObj(DisplayObject *disObj,DisplayObject *child){
    child->parent = disObj;
    child->deep = disObj->childrenNum;
    disObj->childrenNum ++;
}

//添加显示对象到舞台
void addChild(uint8_t num,DisplayObject *disObj){
    // uint16_t len = stage.movieclipArr[num].length;
    // stage.movieclipArr[num].disObjArr[len] = disObj;
    stage.movieclipArr[num].length ++;
    // disObj->deep = num;
    addChildToObj(&stage.movieclipArr[num].disObj,disObj);
}


DisplayObject* getChildAt(DisplayObject *disObj,uint16_t num){
    for(uint16_t i=0;i<displayObjMaxNum;i++){
        if(displayMc[i].parent == disObj){
            if(displayMc[i].deep == num){
                return &displayMc[i];
            }
        }
    }
    return NULL;
}


//释放DisplayObject及其所有子对象占用的内存
void removeChild(DisplayObject *disObj){
    if(!disObj)return;
    while(disObj->childrenNum){
        removeChild(getChildAt(disObj,0));
    }
    
    if(disObj->ac){
        if(disObj->ac->Type){
            disObj->ac->Type = 0;
            disObj->ac->p = NULL;
            disObj->ac->disObj = NULL;
        }
        
    }

    if(disObj->type == 4 || disObj->type == 6){
        my_free(disObj->p);
    }

    //查询当前显示对象在所在层级的显示对象数组中的位置，并将其后面的对象前移覆盖它
    if(disObj->parent){
        for(uint16_t i=disObj->deep;i<disObj->parent->childrenNum-1;i++){
            DisplayObject* disObjTemp = getChildAt(disObj->parent,i+1);
            if(disObjTemp)disObjTemp->deep--;
        }
        disObj->parent->childrenNum --;
        if(disObj->parent->type == 0xFE){
            //如果是主层
            stage.movieclipArr[disObj->parent->deep].length --;
        }
    }
    deleteDisplayObject(disObj); 
}

//删除对象里的所有子对象
void removeAllChildForObj(DisplayObject *disObj){
    if(!disObj)return;
    for(uint16_t i=0;i<displayObjMaxNum;i++){
        if(displayMc[i].parent == disObj){
            removeChild(&displayMc[i]);
        }
    }
}
//删除指定深度的对象
void removeChildFor(DisplayObject *disObj,uint16_t deep){
    if(!disObj)return;
    removeChild(getChildAt(disObj,deep));
}
void removeAllLayerChild(uint8_t layer){
    for(uint16_t i=0;i<displayObjMaxNum;i++){
        if(displayMc[i].parent == &stage.movieclipArr[layer].disObj){
            removeChild(&displayMc[i]);
        }
    }
}
//完整移除DisplayObject
void removeChildAt(uint16_t layer,uint16_t deep){
    if(stage.movieclipArr[layer].length>0){
        stage.movieclipArr[layer].length --;
        removeChild(getChildAt(&stage.movieclipArr[layer].disObj,deep));
    }
    
    //查询当前显示对象在所在层级的显示对象数组中的位置，并将其后面的对象前移覆盖它
    // uint8_t deep = 0;
    // for(uint16_t i=0;i<stage.movieclipArr[disObj->deep].length;i++){
    //     if(deep){
    //         stage.movieclipArr[disObj->deep].disObjArr[i] = stage.movieclipArr[disObj->deep].disObjArr[i+1];
    //     }else{
    //         if(disObj == stage.movieclipArr[disObj->deep].disObjArr[i]){
    //             deep = 1;
    //             stage.movieclipArr[disObj->deep].disObjArr[i] = stage.movieclipArr[disObj->deep].disObjArr[i+1];
    //         }
    //     }
    // }
    // stage.movieclipArr[disObj->deep].disObjArr[stage.movieclipArr[disObj->deep].length-1] = NULL;
    // stage.movieclipArr[disObj->deep].length --;
    // //移除显示对象
    // removeChildren(disObj);
    
}

void clearEnterFunction(void){
    enterFun = NULL;
}

void resetEnterFunction(void (*fun)(void)){
    enterFun = fun;
}

// 缓动函数
float fsCubicEaseIn(float p)
{
    return p * p * p;
}

float fsCubicEaseOut(float p)
{
    float f = (p - 1);
    return f * f * f + 1;
}

float fsCubicEaseInOut(float p)
{
    if(p < 0.5f){
        return 4 * p * p * p;
    }else{
        float f = ((2 * p) - 2);
        return 0.5f * f * f * f + 1;
    }
}

float fsSineEaseInOut(float p)
{
    if(p==1)return 1;
    return 0.5f * (1 - cosf(p * 3.1415f));
}

float fsSineEaseIn(float p)
{
    return  sinf((0.5f *p - 1) * 3.1415f) + 1;
}

float fsSineEaseOut(float p)
{
    return  sinf(0.5f *p * 3.1415f);
}

float fsEaseNone(float p)
{
    return  p;
}

float fsEaseDo(float val,EaseType ease){
    switch(ease){
        case fs_Cubic_EaseIn:
        return fsCubicEaseIn(val);
        case fs_Cubic_EaseOut:
        return fsCubicEaseOut(val);
        case fs_Cubic_EaseInOut:
        return fsCubicEaseInOut(val);
        case fs_Sine_EaseIn:
        return fsSineEaseIn(val);
        case fs_Sine_EaseOut:
        return fsSineEaseOut(val);
        case fs_Sine_EaseInOut:
        return fsSineEaseInOut(val);
        case fs_EaseNone:
        return val;
        case fs_EaseColor:
        return val;
    }
    return 0;
}

void TweenConvertOne(uint8_t num){
    if(TweenArr[num].esae == fs_EaseColor){
        short sr = (TweenArr[num].startValue >> 11) & 0x1F;
        short sg = (TweenArr[num].startValue >> 5)  & 0x3F;
        short sb = TweenArr[num].startValue & 0x1F;
        short er = (TweenArr[num].targetValue >> 11) & 0x1F;
        short eg = (TweenArr[num].targetValue >> 5)  & 0x3F;
        short eb = TweenArr[num].targetValue & 0x1F;
        float bl = fsEaseDo((float)TweenArr[num].stepNow*100/TweenArr[num].stepAll/100,fs_EaseNone);
        short nr = sr+bl*(er-sr);
        short ng = sg+bl*(eg-sg);
        short nb = sb+bl*(eb-sb);
        *TweenArr[num].axis = (nr << 11) | (ng << 5) | nb;
    }else{
        int axisDistance = TweenArr[num].targetValue-TweenArr[num].startValue;
        int axisOffset = ceilf(fsEaseDo((float)(TweenArr[num].stepNow*100/TweenArr[num].stepAll)/100,TweenArr[num].esae)*axisDistance);
        *TweenArr[num].axis = TweenArr[num].startValue + axisOffset;
    }
    
    TweenArr[num].stepNow ++;
}

void TweenConvert(void){
    for(uint8_t i= 0;i<MaxTweenNum;i++){
        if(TweenArr[i].runing){
            if(TweenArr[i].delay){
                TweenArr[i].delay --;
                continue;
            }
            if(TweenArr[i].stepAll==0){
                if(TweenArr[i].axis){
                    *TweenArr[i].axis = TweenArr[i].targetValue;
                }
                TweenArr[i].runing = 0;
                if(TweenArr[i].fun){
                    TweenArr[i].fun();
                }
            }else if(TweenArr[i].stepNow<=TweenArr[i].stepAll){
                TweenConvertOne(i);
            }else{
                TweenArr[i].runing = 0;
            }
        }
    }
}

uint8_t DelayFunction(void(*fun)(void),float delay){
    uint8_t reNum = Tween(NULL,0,0,delay,fs_EaseNone);
    TweenArr[reNum].fun = fun;
    return reNum;
}

void clearDelayFunction(void(*fun)(void)){
    for(uint8_t i= 0;i<MaxTweenNum;i++){
        if(TweenArr[i].runing){
            if(TweenArr[i].fun == fun){
                TweenArr[i].runing = 0;
                return;
            }
        }
    }
}

uint8_t Tween(short *axis,float time,short value,float delay,EaseType ease){
    for(uint8_t i= 0;i<MaxTweenNum;i++){
       if(TweenArr[i].runing&&TweenArr[i].axis == axis&&axis){
           if(!time&&!delay){
               *TweenArr[i].axis = value;
               TweenArr[i].runing = 0;
           }else{
               TweenArr[i].targetValue = value;
               TweenArr[i].startValue = *axis;
               TweenArr[i].delay = delay*TweenArr[i].frame;
               TweenArr[i].stepAll = time*TweenArr[i].frame;
               TweenArr[i].stepNow = 0;
               TweenArr[i].esae = ease;
               if(!delay){
                TweenConvertOne(i);
               }
           }
           return i;
       }
   }

    for(uint8_t i= 0;i<MaxTweenNum;i++){
        if(!time&&!delay){
            *axis = value;
            TweenArr[i].runing = 0;
        }else{
           if(!TweenArr[i].runing){
                if(axis){
                    TweenArr[i].startValue = *axis;
                }
                TweenArr[i].axis = axis;
                TweenArr[i].targetValue = value;
                TweenArr[i].esae = ease;
                TweenArr[i].stepAll = time*fps;
                TweenArr[i].stepNow = 0;
              
                TweenArr[i].runing = 1;
                TweenArr[i].frame = fps;
                TweenArr[i].delay = delay*TweenArr[i].frame;
                TweenArr[i].fun = NULL;
                return i;
           }
      }
   }
   return 0;
}

void deleteAllTween(void){
    for (uint16_t i = 0; i < MaxTweenNum; i++) {
        TweenArr[i].runing = 0;
    }
}

void setKeyFun(KeyName keyNum,void (*keyFunTemp)(void)){
	keyFun[keyNum] = keyFunTemp;
}

void doKeyFun(KeyName keyNum){
    if(keyFun[keyNum])keyFun[keyNum]();
}

void contKeyState(void){
    if(KeyD0)doKeyFun(KM1);
    if(KeyD1)doKeyFun(KM2);
    if(KeyD2)doKeyFun(KM3);

    if(KeyU0)doKeyFun(KMU1);
    if(KeyU1)doKeyFun(KMU2);
    if(KeyU2)doKeyFun(KMU3);

    if(KeyD3)doKeyFun(KML);
    if(KeyD4)doKeyFun(KMR);
    if(KeyU3)doKeyFun(KMUL);
    if(KeyU4)doKeyFun(KMUR);
}

void update(void){
    contKeyState();
    TweenConvert();
    if(enterFun)enterFun();
    FrameRate ++;
}

// 获取一个未被使用的DisplayObject
// DisplayObject* getDisplayObject(void){
//     DisplayObject *displayObject = my_malloc(sizeof(DisplayObject));
//     displayObject->childNum = 0;
//     displayObject->offsetx = 0;
//     displayObject->offsety = 0;
//     displayObject->ac = NULL;
//     displayObject->parent = NULL;
//     return displayObject;
// }
// 获取一个未被使用的DisplayObject
DisplayObject* getDisplayObject(void){
    for(uint16_t i=0;i<displayObjMaxNum;i++){
        if(!displayMc[i].type){
            displayMc[i].parent = NULL;
            displayMc[i].ac = NULL;
            // displayMc[i].children = NULL;
            displayMc[i].childrenNum = 0;
            displayMc[i].nativeIcon = 0;
            displayMc[i].offsetx = 0;
            displayMc[i].offsety = 0;
            return &displayMc[i];
        }
    }
    return &displayMc[displayObjMaxNum-1];
}

//添加数字
DisplayObject* newNumber(uint8_t size,float num,char dot,uint16_t $color,short x,short y){
    DisplayObject *disObj = getDisplayObject();
    disObj->f = disObjftemp;
    disObj->type = 2;
    disObj->visible = 1;
    disObj->alpha = 15;
    disObj->size = size;
    disObj->num = num;
    disObj->dot = dot;
    disObj->color = $color;
    disObj->x = x;
    disObj->y = y;
    disObj->width = 0;
    disObj->height = 0;
    disObj->maskEnable = 1;
    disObj->dir = 0;
    return disObj;
}

//添加中文
DisplayObject* newChar(uint8_t size,const void *f,uint16_t $color,short x,short y){
    DisplayObject *disObj = getDisplayObject();
    disObj->type = 1;
    disObj->visible = 1;
    disObj->alpha = 15;
    disObj->size = size;
    disObj->f = (char *)f;
    disObj->color = $color;
    disObj->x = x;
    disObj->y = y;
    disObj->width = 0;
    disObj->height = 0;
    disObj->maskEnable = 1;
    return disObj;
}

//修改4角图像的尺寸
void setPic4Extend(  DisplayObject *disObj, uint16_t extendW, uint16_t extendH)
{
    if(disObj == NULL) return; 
    if(disObj->type != 6) return; 
    if(disObj->p == NULL) return; 

    uint16_t srcW = getPicWidth( disObj->p,  0); 
    uint16_t srcH = getPicHeight( disObj->p, 0); 
    //--------------------------------
    // 保存拓展尺寸
    //--------------------------------

    disObj->offsetx =  extendW;

    disObj->offsety = extendH;


    //--------------------------------
    // 更新最终对象尺寸
    //--------------------------------

    disObj->width = srcW * 2 + extendW; 
    disObj->height = srcH * 2 + extendH;
}

//4 角框
DisplayObject* newPic4(uint8_t *p,uint16_t extendW,uint16_t extendH,short x,short y)
{
    if(p == NULL) return NULL;

    // type=6 只支持16位RGB565
    if(p[0] != 16) return NULL;
    DisplayObject *disObj = getDisplayObject();
    if(disObj == NULL) return NULL;

    uint16_t srcW = getPicWidth(p, 0);
    uint16_t srcH = getPicHeight(p, 0);
    //--------------------------------
    // 基础参数
    //--------------------------------
    disObj->p = p;
    disObj->type = 6;
    disObj->picType = 16;
    disObj->visible = 1;
    disObj->alpha = 15;
    disObj->color = COLORNONE;
    disObj->x = x;
    disObj->y = y;
    //--------------------------------
    // offsetx / offsety
    // 用作中间拓展尺寸
    //--------------------------------

    disObj->offsetx = extendW;
    disObj->offsety = extendH;
    //--------------------------------
    // 最终显示尺寸
    //--------------------------------
    disObj->width = srcW * 2 + extendW;
    disObj->height = srcH * 2 + extendH;
    disObj->maskEnable = 1;
    disObj->dir = 0;
    //--------------------------------
    // 第一帧像素数据偏移
    //--------------------------------
    disObj->frameOffset = getPicOffset(0, p);
    disObj->f =(char *)(p + disObj->frameOffset);
    return disObj;
}

static FAST_CODE void render_16bit_4corner_image( DisplayObject *disObj, int16_t mcx, int16_t mcy, uint8_t mcNum)
{
    //--------------------------------
    // type=6只支持16位RGB565
    //--------------------------------

    if(disObj->p[0] != 16)
        return;
    //--------------------------------
    // 原始1/4图片尺寸
    //--------------------------------

    uint16_t srcW = getPicWidth(disObj->p, 0); 
    uint16_t srcH = getPicHeight(disObj->p, 0); 

    //--------------------------------
    // offsetx / offsety
    // 作为中间拓展尺寸
    //--------------------------------

    uint16_t extendW = disObj->offsetx;
    uint16_t extendH = disObj->offsety;

    //--------------------------------
    // 对象实际位置
    //--------------------------------
    int16_t objX = mcx + disObj->x;
    int16_t objY =  mcy + disObj->y;
    //--------------------------------
    // 使用DisplayObject实际尺寸裁剪
    //--------------------------------

    ClipRect clip =  calculate_clip_rect(objX,objY,disObj->width,disObj->height,mcNum);
    if(!clip.valid)return;

    //--------------------------------
    // PIC像素数据
    //--------------------------------

    uint16_t *src = (uint16_t *) ( disObj->p + disObj->frameOffset );
    //--------------------------------
    // Alpha
    //--------------------------------
    uint8_t alpha =  disObj->alpha;


    //--------------------------------
    // 裁剪后对应对象内部的起始位置
    //
    // 如果对象x=-20
    // clip.left=0
    // startX=20
    //
    // 自动跳过左边不可见的20像素
    //--------------------------------

    int16_t startX = clip.left -  objX; 
    int16_t startY = clip.top -  objY; 
    int16_t drawW = clip.right - clip.left; 
    int16_t drawH =  clip.bottom -  clip.top;


    //--------------------------------
    // 中间区域结束位置
    //
    // 结构：
    //
    // 0        srcW
    // | 左图    |
    //          | extendW |
    //                    | 右图 |
    //
    //--------------------------------

    uint16_t rightStart =  srcW + extendW; 
    uint16_t bottomStart = srcH + extendH;

    //--------------------------------
    // 开始渲染
    //--------------------------------
    for(int16_t dy = 0; dy < drawH; dy++)
    {
        //--------------------------------
        // 当前像素在完整type=6对象中的Y坐标
        //--------------------------------
        uint16_t dstY =  startY + dy;
        //--------------------------------
        // 目标Y映射到原始1/4图片Y
        //--------------------------------
        uint16_t srcY;
        if(dstY < srcH)
        {
            //--------------------------------
            // 上半部分
            // 正常读取
            //--------------------------------

            srcY =  dstY;
        }
        else if(dstY < bottomStart)
        {
            //--------------------------------
            // 中间纵向拓展区域
            //
            // 延伸原图最下面一行
            //--------------------------------

            srcY = srcH - 1;
        }
        else
        {
            //--------------------------------
            // 下半部分
            //
            // 垂直镜像
            //--------------------------------

            uint16_t localY = dstY - bottomStart;  
            srcY =  srcH - 1 - localY;
        }


        //--------------------------------
        // 当前 framebuffer 行
        //--------------------------------

        uint16_t *fb = &picCache[ ( clip.top + dy - rendSY ) * rendW + clip.left - rendSX ];


        //--------------------------------
        // 渲染一行
        //--------------------------------

        for(int16_t dx = 0; dx < drawW; dx++)
        {
            //--------------------------------
            // 当前像素在完整对象中的X
            //--------------------------------
            uint16_t dstX = startX + dx;
            //--------------------------------
            // 目标X映射到原始1/4图片X
            //--------------------------------

            uint16_t srcX;
            if(dstX < srcW)
            {
                //--------------------------------
                // 左半部分
                //--------------------------------
                srcX =  dstX;
            }
            else if(dstX < rightStart)
            {
                //--------------------------------
                // 中间横向拓展
                //
                // 延伸原图最右一列
                //--------------------------------
                srcX = srcW - 1;
            }
            else
            {
                //--------------------------------
                // 右半部分
                //
                // 水平镜像
                //--------------------------------
                uint16_t localX = dstX - rightStart;
                srcX = srcW - 1 - localX;
            }


            //--------------------------------
            // 读取PIC原始像素
            //--------------------------------

            uint16_t color = src[(uint32_t)srcY * srcW + srcX ];
            //--------------------------------
            // 透明色跳过
            //--------------------------------

            if(color ==  0x0001)
            {
                continue;
            }


            //--------------------------------
            // 输出
            //
            // 直接调用现有set_pixel_fast
            // 保证和render_16bit_image
            // 字节序、alpha处理一致
            //--------------------------------

            set_pixel_fast( clip.left + dx, clip.top + dy, color, alpha);
        }
    }
}
//获取动画帧数据偏移
uint16_t getPicOffset(uint8_t frame,uint8_t* p){
    uint16_t reOffset = 0;
    if(frame<p[1]){
        uint16_t ff = frame*basicOffset;
        if(!p[4+ ff]&&!p[5+ff])frame = p[3+ff]-1;
        for(uint8_t i=0;i<frame;i++){
            uint16_t w = p[2+ i*basicOffset]<<8|p[3+ i*basicOffset];
            uint16_t h = p[4+ i*basicOffset]<<8|p[5+ i*basicOffset];
            if(!h)continue;
            if(p[0]==16)reOffset += w*h*2;
            else if(p[0]==13)reOffset += w*h*2;
            else if(p[0]==8)reOffset += w*h;
            else if(p[0]==4)reOffset += (uint16_t)((float)(w*h)/2+0.9f)+30;
            else if(p[0]==2)reOffset += (uint16_t)((float)(w*h)/4+0.9f)+6;
            else if(p[0]==1)reOffset += (uint16_t)((float)(w*h)/8+0.9f);
            else if(p[0]==0)reOffset += w;
        }
    }
    return p[1]*basicOffset+2+reOffset;
}

void refrushPicVal(DisplayObject *disObj,uint8_t frame){
    if(disObj->p){
        disObj->type = 4;
        disObj->visible = 1;
        disObj->alpha = 15;
        disObj->picType = disObj->p[0];
        disObj->width = getPicWidth(disObj->p,frame);
        disObj->height = getPicHeight(disObj->p,frame);
        disObj->maskEnable = 1;
        disObj->dir = 0;
        disObj->color = COLORNONE;
        disObj->frameOffset = getPicOffset(frame,disObj->p);
        disObj->f = (char*)(disObj->p + disObj->frameOffset);
    }
}

DisplayObject* newPic(const void *p,short x,short y){
    DisplayObject *disObj = getDisplayObject();
    disObj->p = (uint8_t *)p;
    disObj->x = x;
    disObj->y = y;
    
    refrushPicVal(disObj,0);    
    return disObj;
}

//新建图像并指定跳到某一帧
DisplayObject* newPicFrame(uint8_t *p,uint8_t frame,short x,short y){
    DisplayObject *disObj = getDisplayObject();
    disObj->p = p;
    disObj->x = x;
    disObj->y = y;
    
    refrushPicVal(disObj,frame);    
    return disObj;
}

void updatePicData(DisplayObject *disObj,uint8_t *p){
    if(p!=NULL){
        disObj->visible = true;
        if(disObj->p != p){
            disObj->p = p;
            refrushPicVal(disObj,0);
        }
    }else{
        disObj->visible = false;
    }
}

DisplayObject* newPicSingle(const void *p,uint16_t $color,short x,short y){
    DisplayObject *disObj = newPic(p,x,y);
    disObj->color = $color;
    return disObj;
}

//设置矩形范围
void setRect(Rect* rect,short x,short y,short width,short height){
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
}
Rect rect[10];
//获取遮罩矩形
Rect* getRect(short x,short y,short $w,short $h){
    Rect *rect = my_malloc(sizeof(Rect));
    rect->x = x;
    rect->y = y;
    rect->width = $w;
    rect->height = $h;
    return rect;
}

DisplayObject* newNativePic(const void *p,short x,short y){
    DisplayObject *icon = newPic(p,x,y);
    icon->nativeIcon = 1;
    return icon;
}

DisplayObject* newNativePicSingle(const void *p,uint16_t color,short x,short y){
    DisplayObject *icon = newPicSingle(p,color,x,y);
    icon->nativeIcon = 1;
    return icon;
}

DisplayObject* newRect(uint16_t $color,short $w,short $h,short x,short y){
    DisplayObject *disObj = getDisplayObject();
    disObj->type = 3;
    disObj->visible = 1;
    disObj->alpha = 15;
    disObj->width = $w;
    disObj->height = $h;
    disObj->color = $color;
    disObj->x = x;
    disObj->y = y;
    disObj->maskEnable = 1;
    return disObj;
}
DisplayObject* newEmpty(short x,short y){
    DisplayObject *disObj = getDisplayObject();
    disObj->type = 0xFA;
    disObj->visible = 1;
    disObj->x = x;
    disObj->y = y;
    disObj->maskEnable = 1;
    disObj->childrenNum = 0;
    disObj->alpha = 15;
    return disObj;
}
DisplayObject* newCurve(uint8_t *p,uint16_t $color,short x,short y,uint8_t $h){
    DisplayObject *disObj = getDisplayObject();
    disObj->p = p;
    disObj->type = 4;
    disObj->visible = 1;
    disObj->alpha = 15;
    disObj->width = getPicWidth(p,0);
    disObj->height = $h;
    disObj->color = $color;
    disObj->x = x;
    disObj->y = y;
    disObj->maskEnable = 1;
    disObj->frameOffset = getPicOffset(0,disObj->p);
    return disObj;
}

DisplayObject* newLine(uint16_t $color,short sx,short sy,short ex,short ey){
    DisplayObject *disObj = getDisplayObject();
    disObj->type = 5;
    disObj->visible = 1;    
    disObj->alpha = 15;
    disObj->color = $color;
    disObj->x = sx;
    disObj->y = sy;
    disObj->width = ex;
    disObj->height = ey;
    disObj->maskEnable = 1;
    disObj->size = 1;
    return disObj;
}

void setPColor1(uint8_t *p,short x,short y){
    uint16_t w = getPicWidth(p,0);
    uint16_t h = getPicHeight(p,0);
    if(x<w&&y<h&&x>=0&&y>=0){
        uint32_t pixNumByte = y*w+x;
        uint8_t pixNumBit = pixNumByte%8;
        p[basicOffset+2+pixNumByte/8] |= 1<<pixNumBit;
    }
}

void Draw_Circle(uint8_t *p,short xc, short yc, short x, short y,uint8_t angleType)
{
    if(angleType&0x01) setPColor1(p,xc + x, yc + y);
    if(angleType&0x02) setPColor1(p,xc - x, yc + y);
    if(angleType&0x04) setPColor1(p,xc + x, yc - y);
    if(angleType&0x08) setPColor1(p,xc - x, yc - y);
    if(angleType&0x10) setPColor1(p,xc + y, yc + x);
    if(angleType&0x20) setPColor1(p,xc - y, yc + x);
    if(angleType&0x40) setPColor1(p,xc + y, yc - x);
    if(angleType&0x80) setPColor1(p,xc - y, yc - x);
}

void PIC_DrawCircle(uint8_t *p,short xc, short yc, uint8_t r, uint8_t mode,uint8_t angleType)
{
    short x = 0, y = r, yi;
    short d = 3 - 2 * r;
    if (mode) {
        while (x <= y) {
            for (yi = x; yi <= y; yi++) {
                Draw_Circle(p,xc, yc, x, yi,angleType);
            }
            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    } else {
        while (x <= y) {
            Draw_Circle(p,xc, yc, x, y,angleType);
            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
}

void PIC_DrawRect(uint8_t *p,short x, short y, uint8_t w, uint8_t h){
    for(uint8_t i=0;i<h;i++){
        for(uint8_t t=0;t<w;t++){
            setPColor1(p,x+t,y+i);
        }
    }
}

void PIC_DrawHLine(uint8_t *p, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{    
    if(sx == ex){
        // 处理可能的负长度
        if(sy > ey){
            uint16_t temp = sy;
            sy = ey;
            ey = temp;
        }
        for (uint16_t i = sy; i <= ey; i++) {
            setPColor1(p, sx, i);
        }
    }else if(sy == ey){
        // 处理可能的负长度
        if(sx > ex){
            uint16_t temp = sx;
            sx = ex;
            ex = temp;
        }
        for (uint16_t i = sx; i <= ex; i++) {
            setPColor1(p, i, sy);
        }
    }
}

//圆角矩形
DisplayObject* newRoundRectDisObj(uint16_t $color,uint16_t $w,uint16_t $h,short $x,short $y,uint8_t r,uint8_t filled){
    uint8_t *p = my_malloc(basicOffset+2+($w* $h/8+1));
    if(!p){
        printf("newRoundRectDisObj ERROR\n");
        return NULL;
    }
    memset(p,0,basicOffset+2+($w* $h/8+1));
    p[0] = 1;
    p[1] = 1;
    p[2] = $w<<8&0xFF;
    p[3] = $w&0xFF;
    p[4] = $h<<8&0xFF;
    p[5] = $h&0xFF;
    p[6] = p[7] = 0x7f;
    p[8] = 0;
    PIC_DrawCircle(p,r,r,r,filled,0x88);
    PIC_DrawCircle(p,$w-r-1,r,r,filled,0x44);
    PIC_DrawCircle(p,r, $h-r-1,r,filled,0x22);
    PIC_DrawCircle(p,$w-r-1, $h-r-1,r,filled,0x11);
    if(filled){
        PIC_DrawRect(p,r,0,$w-r-r, $h);
        PIC_DrawRect(p,0,r,r, $h-r-r);
        PIC_DrawRect(p,$w-r,r,r, $h-r-r);
    }else{
        PIC_DrawHLine(p,r,0,$w-r-1,0);
        PIC_DrawHLine(p,r,$h-1,$w-r-1,$h-1);
        PIC_DrawHLine(p,0,r,0,$h-r-1);
        PIC_DrawHLine(p,$w-1,r,$w-1,$h-r-1);
    }
    
    return newPicSingle(p,$color,$x,$y);
}

DisplayObject* newProgress(uint16_t $color,uint8_t $w,uint8_t $h,short $x,short $y,uint8_t $r){
    DisplayObject* pBg = newRoundRectDisObj(0x0000,$w,$h,-2,-2,$r,1);
    DisplayObject* pFBg = newRoundRectDisObj(0xFFFF,$w-4,$h-4,0,0,$r-2,1);
    DisplayObject* pFg = newRoundRectDisObj($color,$w-4,$h-4,$x,$y,$r-2,1);
    addChildToObj(pFg,pFBg);
    addChildToObj(pFBg,pBg);
    return pFg;
}

void setProgress(DisplayObject *pObj,uint8_t pr){
    if(pr>100)pr = 100;
    pObj->width = getPicWidth(pObj->p,0)*(pr/100.0f);
}

void reTxtframeAlign(DisplayObject* $txt,char *$val){
    if($val != NULL){
        $txt->f = $val;
    }
    
    switch ($txt->dir)
    {
        case alignLeft:
            $txt->offsetx = 0;
        break;
        case alignMiddle:
            $txt->offsetx = $txt->width/2 - strlen($txt->f)*($txt->size/2)/2-$txt->size/2;
        break;
        case alignRight:
            $txt->offsetx = $txt->width - strlen($txt->f)*($txt->size/2)-$txt->size;
        break;
        default:
        break;
    }
}

void reTxtframeAlignINT(DisplayObject* $txt,int $val){
    sprintf($txt->f,"%d",$val);
    reTxtframeAlign($txt,$txt->f);
}

DisplayObject* newTxtframe(uint8_t $size,short $txtLen,short $x,short $y,char* $text,align $align,uint16_t $txtColor,uint16_t $bgColor,uint16_t $borderColor){
    DisplayObject* txt = newChar($size,$text,$txtColor,$x,$y);
    DisplayObject* pFg = newRoundRectDisObj($bgColor,$size/2*$txtLen+$size*1.8+1,$size+(uint8_t)($size*0.1f)*2,-$size*1.8/4-2,-$size*0.1f,$size/3,1);
    
    txt->width = pFg->width;
    addChildToObj(txt,pFg);
    if($borderColor!=COLORNONE){
        DisplayObject* pBg = newRoundRectDisObj($borderColor,$size/2*$txtLen+$size*1.8+4,$size+(uint8_t)($size*0.1f)*1.8/4-2+4,-2,-2,$size/3+2,1);
        addChildToObj(pFg,pBg);
    }
    txt->dir = $align;
    txt->height = 0;
    reTxtframeAlign(txt,$text);
    return txt;
}

//显示或隐藏选择条
void setMenuListBg(DisplayObject *disObj,uint8_t $boo){
    DisplayObject *temp = (DisplayObject*)disObj->p;//选择底左
    DisplayObject *tempR = ((DisplayObject*)temp->f);//选择底右
    if($boo){
        disObj->num = (uint8_t)disObj->num&0x7F;
        temp->visible = true;
        tempR->visible = true;
    }else{
        disObj->num = (uint8_t)disObj->num|0x80;
        temp->visible = false;
        tempR->visible = false;
    }
}
//设置列表模组的当前序号
//&num 的第8位为是否使能选择(是否出现选择条),1为不显示,0为显示
void setMenuListY(DisplayObject *disObj,uint8_t $num){
    if(disObj == NULL)return;
    DisplayObject *temp = (DisplayObject*)disObj->p;//选择底左
    DisplayObject *tempR = ((DisplayObject*)temp->f);//选择底右
    
    uint8_t size = disObj->size;
    uint16_t listH = disObj->dir ? disObj->dir : (size+size/3*2);
    uint8_t lenAll = disObj->dot;
    if($num>>7){
        $num = $num&0x7F;
        temp->visible = false;
        tempR->visible = false;
    }else{
        temp->visible = true;
        tempR->visible = true;
        temp->x = temp->offsetx;
        tempR->x = -temp->offsetx*3;
        Tween(&temp->x,.3,temp->offsetx*2,0,fs_Sine_EaseOut);
        Tween(&tempR->x,.6,-temp->offsetx*4,0,fs_Cubic_EaseOut);
        temp->y = tempR->y = $num * listH;
    }
    
    
    MovieClip *mc = &stage.movieclipArr[disObj->parent->parent->deep];
    if(lenAll*listH > mc->Mask->height){       
        if($num == 0){
            //从最后一行回到第一行
            disObj->frameOffset = disObj->width;
        }
        else if(disObj->num == 0 && $num == lenAll-1){    
            //从第一行回到最后行
            uint8_t num = ((lenAll)*listH - mc->Mask->height)/listH;
            disObj->frameOffset = -num*listH;
        }else{
            short goY = ($num+1)*listH + disObj->frameOffset;
            if(goY-listH < mc->Mask->y){
                //如果超过上边
                disObj->frameOffset = (disObj->frameOffset+listH)>disObj->width?disObj->width:(disObj->frameOffset+listH);
            }else if(goY > mc->Mask->y + mc->Mask->height){
                // 如果超过下边
                disObj->frameOffset = mc->Mask->height-listH*($num+0.5);
            }
        }
        Tween(&disObj->parent->y,.5,disObj->frameOffset+5,0,fs_Cubic_EaseOut);
    }
    disObj->num = $num;
}

DisplayObject* addMenuList(const char* const $listTxt[],uint8_t $size,uint16_t $colorTxt,uint16_t $colorBar,uint8_t $length,uint16_t $width,short $MaskSY,short $MaskEY,uint8_t $deep){
    if(!$length)return NULL;
    DisplayObject *mcObj = newEmpty(0,0);
    DisplayObject *reMc[$length];
    DisplayObject *picBgTemp;
    uint8_t r = $size/2-1;
    uint16_t listH = $size == 12 ? 28 : ($size + $size/3*2);
    uint16_t rowBGH = $size == 12 ? 26 : ($size + $size/6*2);
    for(uint8_t i=0;i<$length;++i){
        unsigned lines=1;
        for(const char *p=$listTxt[i];*p;++p) if(*p=='\n') ++lines;
        if(lines > 1){
            unsigned needed=lines*($size == 12 ? 20 : 25)+8;
            if(needed > listH){ listH=needed; rowBGH=needed-2; }
        }
    }

    for(uint8_t i=0;i<$length;i++){
        reMc[i] = newChar($size,$listTxt[i],$colorTxt,i==0?r:0,i==0?$size/6:listH);
        if(i>0){
            addChildToObj(reMc[i-1],reMc[i]);
            addChildToObj(mcObj,newPicSingle(picBgTemp->p,0xB596,-r,listH*i));
        }else{
            picBgTemp = newRoundRectDisObj(0xB596,$width+r,rowBGH,-r,0,r,1);
            addChildToObj(mcObj,picBgTemp);
        }
        
    }

    // DisplayObject *listBg = newRoundRectDisObj($colorBar,$width+r*3,$size+$size/6*2,-r,0,r,1);
    DisplayObject *listBg = newRoundRectDisObj($colorBar,$width-r*3,rowBGH,-r,0,r,1);// 选择底左
    DisplayObject *listBgR = newRoundRectDisObj($colorBar,$width-r*3,rowBGH,r*3,0,r,1);// 选择底右
    listBg->f = (char*)listBgR;
    listBg->offsetx = -r;
    reMc[0]->dir = listH;
    reMc[0]->num = 0;//当前选择的行数
    reMc[0]->frameOffset = 0;//目标Y
    reMc[0]->p = (uint8_t*)listBg;  // 转换为uint8_t*以匹配类型
    reMc[0]->width = $MaskSY;
    reMc[0]->height = $MaskEY;
    addChildToObj(mcObj,listBg);  // 选择底左
    addChildToObj(mcObj,listBgR);  // 选择底右
    addChildToObj(mcObj,reMc[0]);
    addChild($deep,mcObj);
    mcObj->y = $MaskSY+5;
    stage.movieclipArr[$deep].Mask = getRect(0,$MaskSY,winW,$MaskEY-$MaskSY);//newRect(0,winW,$MaskEY-$MaskSY,0,$MaskSY);
    stage.movieclipArr[$deep].MaskFollow = 0;
    reMc[0]->dot = $length;
    setMenuListY(reMc[0],0);
    return reMc[0];
}

uint32_t powNum(uint8_t num){
    if(num == 0)return 1;
    if(num == 1)return 10;
    if(num == 2)return 100;
    if(num == 3)return 1000;
    if(num == 4)return 10000;
    if(num == 5)return 100000;
    if(num == 6)return 1000000;
    if(num == 7)return 10000000;
    return 0;
}

uint8_t getNumBit(int num){
    if(num >= 10000000)return 8;
    if(num >= 1000000)return 7;
    if(num >= 100000)return 6;
    if(num >= 10000)return 5;
    if(num >= 1000)return 4;
    if(num >= 100)return 3;
    if(num >= 10)return 2;
    return 1;
}



void setPicCache(uint16_t *cacheDot,uint16_t color,uint8_t alpha){
    if(alpha==0){
        return;
    }else if(alpha>=15){
        *cacheDot = color>>8|color<<8;
        return;
    }
    
    uint16_t dColor = *cacheDot>>8|*cacheDot<<8;
    
    if(color==dColor){
        return;
    }else{
        uint16_t rb = dColor>>11;
        uint16_t gb = dColor>>5&0x3F;
        uint16_t bb = dColor & 0x1F;

        uint16_t ra = color>>11;
        uint16_t ga = color>>5&0x3F;
        uint16_t ba = color & 0x1F;

        uint16_t r = (ra * alpha + rb * (15 - alpha)) >> 4;
        uint16_t g = (ga * alpha + gb * (15 - alpha)) >> 4;
        uint16_t b = (ba * alpha + bb * (15 - alpha)) >> 4;
        
        uint16_t rgb = (r << 11) | (g << 5) | b;
        *cacheDot = (rgb >> 8) | (rgb << 8);
    }
}

void View_DrawLine(short x1, short y1, short x2, short y2,uint16_t color,uint8_t alpha,uint8_t McNumber)
{
	uint16_t t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 

	delta_x=x2-x1;
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1;
	else if(delta_x==0)incx=0;
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x;
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )
	{  
		if(uRow<rendEX&&uCol<rendEY&&uRow>=rendSX&&uCol>=rendSY){
            if(McNumber != 0xFF){
                if(checkMaskX(uRow,McNumber) && checkMaskY(uCol,McNumber))
                set_pixel_fast(uRow, uCol, color, alpha);
            }else{
                set_pixel_fast(uRow, uCol, color, alpha);
            }
             
        }
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
}

//设置图片颜色数组
void setPicColor(DisplayObject *obj,char* color){
    if(obj->type == 4||obj->type == 0xF4){
        if(obj->picType == 1){
            obj->color = *(color);
        }else if(obj->picType == 2||obj->picType == 4){
            obj->type = 0xF4;
            obj->f = color;
        }
    }
}
// ===================== 优化的DrawPic_disObj（完整版本）=====================
void FAST_CODE DrawPic_disObj(DisplayObject* disObjTemp, int16_t mcx, int16_t mcy, uint8_t McNumber)
{
    if (!disObjTemp || !disObjTemp->visible || disObjTemp->alpha == 0) return;
    
    // 处理子对象
    if (disObjTemp->childrenNum) {
        for (uint16_t i = 0; i < disObjTemp->childrenNum; i++) {
            DisplayObject *objTemp = getChildAt(disObjTemp,i);
            DrawPic_disObj(objTemp, mcx + objTemp->parent->x, mcy + objTemp->parent->y, McNumber);
        }
    }
    
    // 处理动画
    if (disObjTemp->ac && disObjTemp->ac->Type == 1) {
        if (disObjTemp->ac->playing) {            
            if (disObjTemp->ac->totallFrame > 1) {
                if (disObjTemp->ac->frameTime > 0) {
                    disObjTemp->ac->frameTime --;
                } else {
                    if (disObjTemp->ac->currentFrame < disObjTemp->ac->totallFrame-1) {
                        disObjTemp->ac->currentFrame ++;
                    } else {
                        disObjTemp->ac->currentFrame = 0;
                    }
                    resetAcFrameTime(disObjTemp->ac);
                    disObjTemp->ac->scaleXNow = disObjTemp->ac->scaleYNow = 100;
                    disObjTemp->ac->angleCurrent = 0xFFFF;
                }
            }
        }

        if ((disObjTemp->ac->scaleXNow != disObjTemp->ac->scaleX) || 
            (disObjTemp->ac->scaleYNow != disObjTemp->ac->scaleY) || 
            disObjTemp->ac->scaleXNow != 100 || 
            disObjTemp->ac->scaleYNow != 100) {
            if (disObjTemp->ac->scaleX != disObjTemp->ac->scaleXNow || 
                disObjTemp->ac->scaleYNow != disObjTemp->ac->scaleY) {
                refrushScaleAction(disObjTemp->ac);
            }
        } else {
            rotaAction(disObjTemp->ac, disObjTemp->ac->angle);
        }
        mcx -= disObjTemp->width >> 1;
        mcy -= disObjTemp->height >> 1;
    }
    
    short rx, ry;
    short sx, sy;
    uint16_t temp;
    
    switch(disObjTemp->type) {
        case 0xFA: // 空对象
        break;
        case 5: // 线段
            if (disObjTemp->width || disObjTemp->height) {
                short nx = disObjTemp->x + mcx + disObjTemp->offsetx;
                short ny = disObjTemp->y + mcy + disObjTemp->offsety;
                for (rx = 0; rx < disObjTemp->size; rx++) {
                    View_DrawLine(nx + rx, ny, rx + mcx + disObjTemp->width + disObjTemp->offsetx,
                                 mcy + disObjTemp->height + disObjTemp->offsety, 
                                 disObjTemp->color, disObjTemp->alpha,
                                 stage.movieclipArr[McNumber].Mask?McNumber:0xFF);
                                 //待做 在MASK下无法显示线段
                }
            }
            break;
        case 6: 
            render_16bit_4corner_image(disObjTemp, mcx, mcy, McNumber);
            break; 
        case 4: // 图片
        case 0xF4: // 固定颜色图片
            // 图片偏移
            if (disObjTemp->ac) {
                if (disObjTemp->ac->p) {
                    mcx -= (0x7F - (short)disObjTemp->ac->p[6 + disObjTemp->ac->currentFrame * basicOffset]) * (disObjTemp->ac->scaleX / 100.0f);
                    mcy -= (0x7F - (short)disObjTemp->ac->p[7 + disObjTemp->ac->currentFrame * basicOffset]) * (disObjTemp->ac->scaleY / 100.0f);
                }
            }else{
                mcx -= 0x7F - (short)disObjTemp->p[6];
                mcy -= 0x7F - (short)disObjTemp->p[7];
            }
                
            if (disObjTemp->nativeIcon && !stage.movieclipArr[McNumber].Mask &&
                nativeIconDraw &&
                nativeIconDraw(disObjTemp, mcx + disObjTemp->x,
                               mcy + disObjTemp->y)) break;

            if (disObjTemp->p[0] == 0) {
                // 曲线 - 保持原逻辑
                for (rx = 0; rx < disObjTemp->width; rx++) {
                    sx = mcx + disObjTemp->x + rx;
                    if (sx < rendSX || sx >= rendEX) continue;
                    uint8_t *pt = disObjTemp->p + disObjTemp->frameOffset;
                    for (ry = pt[rx]; ry < disObjTemp->height; ry++) {
                        sy = mcy + disObjTemp->y + ry;
                        if (sy < rendSY || sy >= rendEY) break;
                        set_pixel_fast(sx, sy, disObjTemp->color, disObjTemp->alpha);
                    }
                }
            } else {
                // 使用优化渲染
                switch (disObjTemp->picType) {
                    case 16: render_16bit_image(disObjTemp, mcx, mcy, McNumber); break;
                    case 13: render_rgb454_image(disObjTemp, mcx, mcy, McNumber); break;
                    case 8:  render_8bit_image(disObjTemp, mcx, mcy, McNumber); break;
                    case 4:  render_4bit_image(disObjTemp, mcx, mcy, McNumber); break;  // 新增
                    case 2:  render_2bit_image(disObjTemp, mcx, mcy, McNumber); break;  // 新增
                    case 1:  render_1bit_image(disObjTemp, mcx, mcy, McNumber); break;
                    // 其他格式保持原有逻辑
                    default:
                        // 调用原有渲染逻辑
                        break;
                }
            }
            break;
            
        case 3: // 纯色矩形
            render_rectangle(disObjTemp, mcx, mcy, McNumber);
            break;
            
        case 2: // 数字
            sprintf(disObjftemp, "%.*f", disObjTemp->dot, disObjTemp->num);
            reTxtframeAlign(disObjTemp, disObjftemp);
            switch (disObjTemp->dir) {
                case alignLeft:
                    disObjTemp->offsetx = 0;
                    break;
                case alignMiddle:
                    disObjTemp->offsetx = -strlen(disObjftemp) * disObjTemp->size / 2 / 2;
                    break;
                case alignRight:
                    disObjTemp->offsetx = -strlen(disObjftemp) * (disObjTemp->size / 2);
                    break;
            }
            // 继续执行文本渲染
            // 注意：这里没有break，会继续执行case 1的代码
            
        case 1: { // Text stays inside the active square LCD source area.
            const uint8_t *text = (const uint8_t *)disObjTemp->f;
            if (!text || !disObjTemp->size) break;
            const int16_t left = rendH == 240 ? 40 : 75;
            const int16_t right = rendH == 240 ? 280 : 245;
            const int16_t char_w = disObjTemp->size / 2;
            const int16_t max_cols = ((right - left) / char_w) & ~1;
            int16_t cols = 0, max_line_cols = 0, lines = 1;
            int auto_wrapped = 0;
            for (const uint8_t *p = text; *p; ) {
                if (*p == '\n') {
                    if (cols > max_line_cols) max_line_cols = cols;
                    ++lines;
                    cols = 0;
                    ++p;
                    continue;
                }
                const int16_t step = (*p > 0x80 && p[1]) ? 2 : 1;
                if (cols + step > max_cols) {
                    if (cols > max_line_cols) max_line_cols = cols;
                    ++lines;
                    cols = 0;
                    auto_wrapped = 1;
                }
                cols += step;
                p += step;
            }
            if (cols > max_line_cols) max_line_cols = cols;
            int16_t base_x = mcx + disObjTemp->x + disObjTemp->offsetx;
            int16_t base_y = mcy + disObjTemp->y + disObjTemp->offsety;
            if (disObjTemp->maskEnable != 2) {
                if (auto_wrapped) base_x = left;
                else {
                    if (base_x < left) base_x = left;
                    if (base_x + max_line_cols * char_w > right) base_x = right - max_line_cols * char_w;
                }
            }
            if (!stage.movieclipArr[McNumber].Mask) {
                if (base_y < 0) base_y = 0;
                const int16_t text_h = lines * (disObjTemp->size + 2) - 2;
                if (base_y + text_h > rendEY) base_y = rendEY > text_h ? rendEY - text_h : 0;
            }
            int16_t clip_left = left, clip_top = rendSY;
            int16_t clip_right = right, clip_bottom = rendEY;
            const MovieClip *text_mc = &stage.movieclipArr[McNumber];
            if (text_mc->Mask) {
                const int16_t mask_x = text_mc->Mask->x +
                    (text_mc->MaskFollow ? text_mc->x : 0);
                const int16_t mask_y = text_mc->Mask->y +
                    (text_mc->MaskFollow ? text_mc->y : 0);
                if (clip_left < mask_x) clip_left = mask_x;
                if (clip_top < mask_y) clip_top = mask_y;
                if (clip_right > mask_x + text_mc->Mask->width)
                    clip_right = mask_x + text_mc->Mask->width;
                if (clip_bottom > mask_y + text_mc->Mask->height)
                    clip_bottom = mask_y + text_mc->Mask->height;
            }
            if (clip_left >= clip_right || clip_top >= clip_bottom) break;
            if (nativeTextDraw) {
                // A hooked LCD must never silently fall back to the scaled canvas font.
                assert(disObjTemp->alpha == 15 && !disObjTemp->ac);
                const uint8_t drawn = nativeTextDraw((const char *)text, base_x, base_y,
                                                     disObjTemp->color, disObjTemp->size,
                                                     clip_left, clip_top, clip_right, clip_bottom);
                assert(drawn && "native LCD text draw failed");
                if (!drawn) abort();
                break;
            }
            for (ry = 0; ry < disObjTemp->size; ++ry) {
                int16_t col = 0, line = 0;
                for (const uint8_t *p = text; *p; ) {
                    if (*p == '\n') {
                        ++line;
                        col = 0;
                        ++p;
                        continue;
                    }
                    const int chinese = *p > 0x80 && p[1];
                    const int16_t step = chinese ? 2 : 1;
                    if (col + step > max_cols) {
                        ++line;
                        col = 0;
                    }
                    sy = base_y + line * (disObjTemp->size + 2) + ry;
                    sx = base_x + col * char_w;
                    if (sy >= rendSY && sy < rendEY &&
                        (!disObjTemp->maskEnable || checkMaskY(sy, McNumber))) {
                        if (chinese && disObjTemp->size == 12)
                            temp = getFont12((uint8_t *)p, ry);
                        else if (chinese && disObjTemp->size == 16)
                            temp = getFont16((uint8_t *)p, ry);
                        else if (!chinese && *p >= 32) {
                            char *glyph = getFont(disObjTemp->size, *p - 32, ry);
                            temp = disObjTemp->size > 16 ?
                                ((uint8_t)glyph[0] | ((uint16_t)(uint8_t)glyph[1] << 8)) :
                                (uint8_t)glyph[0];
                        } else temp = 0;
                        const uint8_t glyph_w = chinese ? 16 : char_w;
                        for (uint8_t n = 0; n < glyph_w; ++n) {
                            const int16_t px = sx + (chinese ? (n < 8 ? n + 8 : n - 8) : n);
                            if ((temp & 1) && px >= left && px < right &&
                                px >= rendSX && px < rendEX && checkMaskX(px, McNumber))
                                set_pixel_fast(px, sy, disObjTemp->color, disObjTemp->alpha);
                            temp >>= 1;
                        }
                    }
                    col += step;
                    p += step;
                }
            }
            break;
        }

        default:
            break;
    }
}

// ===================== 优化的DrawPic =====================
void FAST_CODE DrawPic(uint8_t mcNum)
{
    MovieClip *mc = &stage.movieclipArr[mcNum];
    
    for (uint16_t i = 0; i < mc->length; i++) {
        DisplayObject *disObj = getChildAt(&mc->disObj,i);
        if (disObj && disObj->type) {
            DrawPic_disObj(disObj, mc->x, mc->y, mcNum);
        } else {
            break;
        }
    }
}

// ===================== 优化的渲染窗口函数 =====================
void FAST_CODE rendWin(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t bg)
{
    if (nativeTextBegin) nativeTextBegin();
    if (nativeIconBegin) nativeIconBegin();
    rendSX = sx;
    rendSY = sy;
    rendEX = ex;
    rendEY = ey;
    rendW = ex - sx;
    rendH = ey - sy;
    
    // 优化背景填充
    static uint16_t last_bg = 0xFEDC;
    if (last_bg != bg) {
        last_bg = bg;
        
        // 填充第一行缓存
        for (uint16_t i = 0; i < winW; i++) {
            picCacheTemp[i * 2] = bg >> 8;
            picCacheTemp[i * 2 + 1] = bg & 0xFF;
        }
    }
    
    // 使用memcpy批量填充
    uint8_t *dst = (uint8_t *)picCache;
    for (uint16_t i = 0; i < rendH; i++) {
        memcpy(dst, picCacheTemp, rendW * 2);
        dst += rendW * 2;
    }
    
    // 渲染各层
    for (uint16_t n = 0; n < moveClipMaxNum; n++) {
        if (stage.movieclipArr[n].visible) {
            DrawPic(n);
        }
    }
    spiSendHd = 1;
}

// ===================== 初始化函数 =====================
void gviewInit(uint16_t $winW, uint16_t $winH, uint16_t $moveClipMaxNum, uint16_t $displayObjMaxNum, uint16_t $MaxTweenNum, uint16_t $MaxActionNum, uint8_t $fps)
{
    winW = $winW;
    winH = $winH;
    fps = $fps;
    
    // 分配内存
    picCache = (uint16_t *)GVIEW_MALLOC((size_t)winW * (winH > 240 ? winH : 240) * sizeof(uint16_t));
    if (!picCache) {
        printf("[GVIEW] Error: Failed to allocate frame buffer!\n");
        return;
    }
    
    picCacheTemp = (uint8_t *)GVIEW_MALLOC(winW * 2);
    if (!picCacheTemp) {
        printf("[GVIEW] Error: Failed to allocate temp buffer!\n");
        free(picCache);
        return;
    }
    
    // 初始化动态内存参数
    // mcDisplayObjectMax = $mcDisplayObjectMax; //每个MovieClip的最大DisplayObject数量
    moveClipMaxNum = $moveClipMaxNum;// MovieClip的最大数量
    displayObjMaxNum = $displayObjMaxNum;// DisplayObject的最大数量
    MaxTweenNum = $MaxTweenNum;// Tween数量
    MaxActionNum = $MaxActionNum;// Action数量
    
    // 分配结构内存
    stage.movieclipArr = (MovieClip *)GVIEW_MALLOC(moveClipMaxNum * sizeof(MovieClip));
    displayMc = (DisplayObject *)GVIEW_MALLOC(displayObjMaxNum * sizeof(DisplayObject));
    TweenArr = (TweenS *)GVIEW_MALLOC(MaxTweenNum * sizeof(TweenS));
    Action = (DisObjAction *)GVIEW_MALLOC(MaxActionNum * sizeof(DisObjAction));
    
    if (!stage.movieclipArr|| !displayMc  || !TweenArr || !Action) {
        printf("[GVIEW] Error: Failed to allocate structure memory!\n");
        if (picCache) free(picCache);
        if (picCacheTemp) free(picCacheTemp);
        if (stage.movieclipArr) free(stage.movieclipArr);
        if (displayMc) free(displayMc);
        if (TweenArr) free(TweenArr);
        if (Action) free(Action);
        return;
    }
    
    // 初始化MovieClip数组
    for (uint16_t i = 0; i < moveClipMaxNum; i++) {
        stage.movieclipArr[i].visible = 1;
        stage.movieclipArr[i].x = 0;
        stage.movieclipArr[i].y = 0;
        // stage.movieclipArr[i].disObjArr = (DisplayObject **)GVIEW_MALLOC(mcDisplayObjectMax * sizeof(DisplayObject *));
        stage.movieclipArr[i].Mask = NULL;
        stage.movieclipArr[i].MaskFollow = 0;
        stage.movieclipArr[i].length = 0;
        stage.movieclipArr[i].disObj.type = 0xFE;
        stage.movieclipArr[i].disObj.deep = i;
    }
    
    // 初始化其他数组
    memset(TweenArr, 0, MaxTweenNum * sizeof(TweenS));
    memset(Action, 0, MaxActionNum * sizeof(DisObjAction));
    memset(displayMc, 0, displayObjMaxNum * sizeof(DisplayObject));
    
    printf("[GVIEW] System initialized successfully\n");
}


// ===================== 清空视图 =====================
void resetView(void (*fun)(void))
{
    softKeyHeaderVisible = 0;
    for (uint16_t i = 0; i < displayObjMaxNum; i++) {
        if (displayMc[i].ac && displayMc[i].p &&
            displayMc[i].p != displayMc[i].ac->p)
            my_free(displayMc[i].p);
        deleteDisplayObject(&displayMc[i]);
    }

    for (uint16_t i = 0; i < moveClipMaxNum; i++) {
        stage.movieclipArr[i].x = 0;
        stage.movieclipArr[i].y = 0;
        stage.movieclipArr[i].length = 0;
        stage.movieclipArr[i].Mask = NULL;
        stage.movieclipArr[i].visible = 1;
        stage.movieclipArr[i].disObj.childrenNum = 0;
    }
    
    deleteAllTween();
    
    enterFun = fun;
    
    for (uint16_t i = 0; i < 7; i++) {
        keyFun[i] = NULL;
    }
    
    for (uint16_t i = 0; i < MaxActionNum; i++) {
        Action[i].Type = 0;
        Action[i].p = NULL;
        Action[i].disObj = NULL;
    }
    for (uint8_t i = 0; i < 10; i++) {
        keyFun[i] = NULL;
    }
    free_all();
}



//获取文本字母长度
uint8_t getTxtLength(const char *txt){
    char* txtTemp = txt;
    uint8_t txtNum = 0;
    while(*txtTemp!=0){
        txtNum ++;
        txtTemp ++;
    }
    return txtNum;
}