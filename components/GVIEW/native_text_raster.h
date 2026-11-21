#pragma once
#include "viewport.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern char *getFont(uint8_t size, uint8_t type, uint8_t line);
extern uint16_t getFont16(uint8_t *s, uint8_t line);

typedef struct {
    LCDViewport viewport;
    int width, height;
    int left, top, right, bottom, stride;
    uint8_t *indices, *rows;
    uint16_t *pixels;
} GVIEWNativeTextSurface;

static inline int gviewTextClipPixel(int edge, int start, int extent, int pixels){
    int pos = ((edge - start) * pixels) / extent;
    if (pos < 0) pos = 0;
    if (pos > pixels) pos = pixels;
    while (pos > 0 && start + ((2 * (pos - 1) + 1) * extent) / (2 * pixels) >= edge) --pos;
    while (pos < pixels && start + ((2 * pos + 1) * extent) / (2 * pixels) < edge) ++pos;
    return pos;
}

// Inline the raster loop so a constant surface kind selects a direct buffer
// store at compile time. C3 keeps its strip buffer and has no per-pixel callback.
static inline __attribute__((always_inline))
uint8_t gviewRasterNativeText(const GVIEWNativeTextSurface *surface,
                            const char *text, int16_t x, int16_t y,
                            uint16_t ink, uint8_t size,
                            int16_t clipLeft, int16_t clipTop,
                            int16_t clipRight, int16_t clipBottom){
    const LCDViewport viewport = surface->viewport;
    int pixelLeft = gviewTextClipPixel(clipLeft, viewport.left, viewport.width, surface->width);
    int pixelRight = gviewTextClipPixel(clipRight, viewport.left, viewport.width, surface->width);
    int pixelTop = gviewTextClipPixel(clipTop, viewport.top, viewport.height, surface->height);
    int pixelBottom = gviewTextClipPixel(clipBottom, viewport.top, viewport.height, surface->height);
    if (pixelLeft < surface->left) pixelLeft = surface->left;
    if (pixelRight > surface->right) pixelRight = surface->right;
    if (pixelTop < surface->top) pixelTop = surface->top;
    if (pixelBottom > surface->bottom) pixelBottom = surface->bottom;
    if (pixelLeft >= pixelRight || pixelTop >= pixelBottom) return 1;
    int originX = ((x - viewport.left) * surface->width + viewport.width/2) / viewport.width;
    const int originY = ((y - viewport.top) * surface->height + viewport.height/2) / viewport.height;
    const int glyphSize = size == 12 ? 16 : size == 18 ? 22 : 20;
    const int asciiWidth = glyphSize / 2;
    const int linePitch = size == 12 ? 20 : size == 18 ? 27 : 25;
    const int maxColumns = ((pixelRight - originX) / asciiWidth) & ~1;
    if (maxColumns < 2) return 1;
    if (size == 16 && !strchr(text, '\n')) {
        const int bytes = strlen(text);
        if (bytes <= maxColumns && abs(x + bytes * 4 - (viewport.left + viewport.width/2)) <= 2) {
            const int originalWidth = (bytes * 8 * surface->width + viewport.width/2) / viewport.width;
            originX += (originalWidth - bytes * asciiWidth) / 2;
        }
    }
    int columns = 0, line = 0;
    for (const uint8_t *p = (const uint8_t *)text; *p; ) {
        if (*p == '\n') { columns = 0; ++line; ++p; continue; }
        const int chinese = *p > 0x80 && p[1];
        const int step = chinese ? 2 : 1;
        if (columns + step > maxColumns) { columns = 0; ++line; }
        if (!chinese && (*p < 32 || *p > 126)) { columns += step; ++p; continue; }
        const int glyphX = originX + columns * asciiWidth;
        const int glyphY = originY + line * linePitch;
        const int glyphWidth = chinese ? glyphSize : asciiWidth;
        const int sourceWidth = chinese ? 16 : 8;
        for (int row = 0; row < glyphSize; ++row) {
            const int py = glyphY + row;
            if (py < pixelTop || py >= pixelBottom) continue;
            const int sourceRow = ((2 * row + 1) * 16) / (2 * glyphSize);
            const uint16_t bits = chinese ? getFont16((uint8_t *)p, sourceRow) :
                                           (uint8_t)*getFont(16, *p - 32, sourceRow);
            for (int col = 0; col < glyphWidth; ++col) {
                const int sourceCol = ((2 * col + 1) * sourceWidth) / (2 * glyphWidth);
                const int bit = chinese ? (sourceCol < 8 ? sourceCol + 8 : sourceCol - 8) : sourceCol;
                const int px = glyphX + col;
                if ((bits & (1u << bit)) && px >= pixelLeft && px < pixelRight) {
                    const int index = (py - surface->top) * surface->stride + px - surface->left;
                    if (surface->pixels) surface->pixels[index] = ink;
                    else surface->indices[index] = (uint8_t)ink;
                    surface->rows[py] = 1;
                }
            }
        }
        columns += step;
        p += step;
    }
    return 1;
}
