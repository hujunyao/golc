#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <android/log.h>
#include <string.h>
#include "SkBitmap.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include "SkRect.h"
#include "freetype-glue.h"
#include "skf-types.h"
#include "flashengine/common/inc/mrc_base.h"

#define FG_TAG "Freetype-Glue"
#define D(...) __android_log_print(ANDROID_LOG_ERROR, FG_TAG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

extern skf_buffer_t *g_skf_buffer;

int32 mrc_drawTextLeft(char* pcText, int16 x, int16 y, mr_screenRectSt rect, mr_colourSt colorst, int flag, uint16 font) {
  SkBitmap bmp = SkBitmap();
  SkRect bounds;
  //bmp.setConfig(SkBitmap::kARGB_8888_Config, w, h, 0);
  bmp.setConfig(SkBitmap::kRGB_565_Config, g_skf_buffer->w, g_skf_buffer->h, 0);
  bmp.setPixels((void *)g_skf_buffer->pixels);
  SkCanvas c(bmp);
  SkPaint pen;
  pen.setARGB(255, colorst.r, colorst.g, colorst.b);
  pen.setAntiAlias(true);
  if(font == MR_FONT_SMALL)
    pen.setTextSize(12);
  else if(font == MR_FONT_MEDIUM)
    pen.setTextSize(18);
  else if(font == MR_FONT_BIG)
    pen.setTextSize(26);

  pen.setTextEncoding(SkPaint::kUTF8_TextEncoding);
  c.drawText(pcText, strlen(pcText), x, y, pen);
}

void mrc_drawRect(int16 x, int16 y, int16 w, int16 h, uint8 r0, uint8 g0, uint8 b0) {
  SkBitmap bmp = SkBitmap();
  SkRect bounds, r;
  SkIRect ir;
  //bmp.setConfig(SkBitmap::kARGB_8888_Config, w, h, 0);
  bmp.setConfig(SkBitmap::kRGB_565_Config, g_skf_buffer->w, g_skf_buffer->h, 0);
  bmp.setPixels((void *)g_skf_buffer->pixels);
  SkCanvas c(bmp);
  SkPaint pen;
  pen.setARGB(255, r0, g0, b0);

  ir.set(x, y, w, h);
  r.set(ir);
  c.drawRect(r, pen);
}

void skia_draw_text(int *pixels, int w, int h, int xoff, int yoff) {
  const char * str = "中国Hello";
  int len = strlen(str);

  SkBitmap bmp = SkBitmap();
  SkRect bounds;
  //bmp.setConfig(SkBitmap::kARGB_8888_Config, w, h, 0);
  bmp.setConfig(SkBitmap::kRGB_565_Config, w, h, 0);
  bmp.setPixels((void *)pixels);
  SkCanvas c(bmp);
  SkPaint pen;
  pen.setARGB(255, 0, 0, 255);
  pen.setAntiAlias(true);
  pen.setTextSize(28);
  pen.setTextEncoding(SkPaint::kUTF8_TextEncoding);
  c.drawText(str, len, 60, 60, pen);
  pen.measureText(str, len, &bounds);
  //D("len = %d, bounds(x = %f, y = %f w = %f, h = %f)\n", len, bounds.left(), bounds.right(), bounds.width(), bounds.height());
  pen.setStyle(SkPaint::kStroke_Style);
  pen.setARGB(128, 255, 0, 128);
  bounds.set(60, 60-bounds.height(), 60+bounds.width(), 60);
  c.drawRect(bounds, pen);
}

void skia_draw_rect(int *pixels, int w, int h, int x, int y, int width, int height) {
  SkBitmap bmp = SkBitmap();
  //bmp.setConfig(SkBitmap::kARGB_8888_Config, w, h, 0);
  bmp.setConfig(SkBitmap::kRGB_565_Config, w, h, 0);
  bmp.setPixels((void *)pixels);
  SkCanvas c(bmp);
  SkPaint paint;
  paint.setARGB(128, 0, 255, 0);
  SkRect r;
  r.set(x, y, width, height);
  c.drawRect(r, paint);
}

#ifdef __cplusplus
}
#endif
