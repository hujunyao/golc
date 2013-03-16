#include <stdio.h>
#include <dlfcn.h>
#include <android/log.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/tttables.h>
#include <freetype/t1tables.h>
#include <freetype/ftlcdfil.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FG_TAG "Freetype-Glue"
#define D(...) __android_log_print(ANDROID_LOG_ERROR, FG_TAG, __VA_ARGS__)
static FT_Error (* _skFT_Init_FreeType)(FT_Library *);
static FT_Error (* _skFT_Open_Face)(FT_Library, const FT_Open_Args*, FT_Long, FT_Face *);
static FT_Error (* _skFT_Done_Face)(FT_Face face);
static FT_Error (* _skFT_Done_FreeType)(FT_Library );
static FT_UInt  (* _skFT_Get_Char_Index)(FT_Face , FT_ULong );
static FT_Error (* _skFT_Load_Glyph)(FT_Face , FT_UInt, FT_Int32);
static void (* _skFT_Outline_Get_CBox)(const FT_Outline*, FT_BBox *);
static FT_ULong (* _skFT_Get_First_Char)(FT_Face , FT_UInt *);
static FT_ULong (* _skFT_Get_Next_Char)(FT_Face , FT_ULong , FT_UInt *);
static FT_Long  (* _skFT_MulFix)(FT_Long, FT_Long);
static FT_Error (* _skFT_Outline_Embolden)(FT_Outline *, FT_Pos);
static void     (* _skFT_Set_Transform)(FT_Face, FT_Matrix *, FT_Vector *);
static void *   (* _skFT_Get_Sfnt_Table)(FT_Face, FT_Sfnt_Tag );
static FT_Error (* _skFT_Outline_Decompose)(FT_Outline *, const FT_Outline_Funcs *, void *);
static FT_Error (* _skFT_GlyphSlot_Own_Bitmap)(FT_GlyphSlot );
static FT_Error (* _skFT_Bitmap_Embolden)(FT_Library , FT_Bitmap *, FT_Pos, FT_Pos);
static FT_Error (* _skFT_Get_Advance)(FT_Face , FT_UInt, FT_Int32, FT_Fixed *);
static FT_Error (* _skFT_Library_SetLcdFilter)(FT_Library , FT_LcdFilter);
static FT_Error (* _skFT_New_Size)(FT_Face , FT_Size *);
static FT_Error (* _skFT_Activate_Size)(FT_Size );
static FT_Error (* _skFT_Set_Char_Size)(FT_Face, FT_F26Dot6, FT_F26Dot6, FT_UInt, FT_UInt);
static FT_Error (* _skFT_Done_Size)(FT_Size );
static void     (* _skFT_Outline_Translate)(const FT_Outline *, FT_Pos, FT_Pos);
static FT_Error (* _skFT_Outline_Get_Bitmap)(FT_Library , FT_Outline *, const FT_Bitmap *);
static FT_Error (* _skFT_Render_Glyph)(FT_GlyphSlot , FT_Render_Mode);
static FT_Error (* _skFT_Get_PS_Font_Info)(FT_Face , PS_FontInfo);
static FT_Error (* _skFT_Set_Charmap)(FT_Face , FT_CharMap);
static FT_Error (* _skFT_Get_Glyph_Name)(FT_Face, FT_UInt, FT_Pointer, FT_UInt);
static FT_Error (* _skFT_Get_Advances)(FT_Face , FT_UInt, FT_UInt, FT_Int32, FT_Fixed *);
static FT_UShort  (* _skFT_Get_FSType_Flags)(FT_Face );
static const char* (* _skFT_Get_X11_Font_Format)(FT_Face );
static const char* (* _skFT_Get_Postscript_Name)(FT_Face );

static void *g_handle = NULL;

int freetype_glue_init() {
  FT_Library lib;

  g_handle = dlopen("/system/lib/libskia.so", RTLD_LAZY);
  if(g_handle == NULL) {
    D("open freetype glue /system/lib/libskia.so failed\n");
    return -1;
  }
  _skFT_Init_FreeType = dlsym(g_handle, "FT_Init_FreeType");
  _skFT_Open_Face = dlsym(g_handle, "FT_Open_Face");
  _skFT_Done_Face = dlsym(g_handle, "FT_Done_Face");
  _skFT_Done_FreeType = dlsym(g_handle, "FT_Done_FreeType");
  _skFT_Get_Char_Index = dlsym(g_handle, "FT_Get_Char_Index");
  _skFT_Load_Glyph = dlsym(g_handle, "FT_Load_Glyph");
  _skFT_Outline_Get_CBox = dlsym(g_handle, "FT_Outline_Get_CBox");
  _skFT_Get_First_Char = dlsym(g_handle, "FT_Get_First_Char");
  _skFT_Get_Next_Char = dlsym(g_handle, "FT_Get_Next_Char");
  _skFT_MulFix = dlsym(g_handle, "FT_MulFix");
  _skFT_Outline_Embolden = dlsym(g_handle, "FT_Outline_Embolden");
  _skFT_Activate_Size = dlsym(g_handle, "FT_Activate_Size");
  _skFT_Set_Transform = dlsym(g_handle, "FT_Set_Transform");
  _skFT_Get_Sfnt_Table = dlsym(g_handle, "FT_Get_Sfnt_Table");
  _skFT_Outline_Decompose = dlsym(g_handle, "FT_Outline_Decompose");
  _skFT_GlyphSlot_Own_Bitmap = dlsym(g_handle, "FT_GlyphSlot_Own_Bitmap");
  _skFT_Bitmap_Embolden = dlsym(g_handle, "FT_Bitmap_Embolden");
  _skFT_Get_Advance = dlsym(g_handle, "FT_Get_Advance");
  _skFT_Library_SetLcdFilter = dlsym(g_handle, "FT_Library_SetLcdFilter");
  _skFT_New_Size = dlsym(g_handle, "FT_New_Size");
  _skFT_Set_Char_Size = dlsym(g_handle, "FT_Set_Char_Size");
  _skFT_Done_Size = dlsym(g_handle, "FT_Done_Size");
  _skFT_Outline_Translate = dlsym(g_handle, "FT_Outline_Translate");
  _skFT_Outline_Get_Bitmap = dlsym(g_handle, "FT_Outline_Get_Bitmap");
  _skFT_Render_Glyph = dlsym(g_handle, "FT_Render_Glyph");
  _skFT_Get_Postscript_Name = dlsym(g_handle, "FT_Get_Postscript_Name");
  _skFT_Get_X11_Font_Format = dlsym(g_handle, "FT_Get_X11_Font_Format");
  _skFT_Get_PS_Font_Info = dlsym(g_handle, "FT_Get_PS_Font_Info");
  _skFT_Get_FSType_Flags = dlsym(g_handle, "FT_Get_FSType_Flags");
  _skFT_Set_Charmap = dlsym(g_handle, "FT_Set_Charmap");
  _skFT_Get_Glyph_Name = dlsym(g_handle, "FT_Get_Glyph_Name");
  _skFT_Get_Advances = dlsym(g_handle, "FT_Get_Advances");

  D("_skFT_Library_SetLcdFilter = %x\n", _skFT_Library_SetLcdFilter);
  return 0;
}

FT_Error skFT_Init_FreeType(FT_Library *lib) {
  return _skFT_Init_FreeType(lib);
}

FT_Error skFT_Open_Face(FT_Library lib, const FT_Open_Args* args, FT_Long fl, FT_Face *face) {
  return _skFT_Open_Face(lib, args, fl, face);
}

FT_Error skFT_Done_Face(FT_Face face) {
  return _skFT_Done_Face(face);
}

FT_Error skFT_Done_FreeType(FT_Library lib) {
  return _skFT_Done_FreeType(lib);
}

FT_UInt  skFT_Get_Char_Index(FT_Face face, FT_ULong ful) {
  return _skFT_Get_Char_Index(face, ful);
}


FT_Error skFT_Load_Glyph(FT_Face face, FT_UInt fui, FT_Int32 fi) {
  return _skFT_Load_Glyph(face, fui, fi);
}

void     skFT_Outline_Get_CBox(const FT_Outline* fol, FT_BBox * fbb) {
  _skFT_Outline_Get_CBox(fol, fbb);
}

FT_ULong skFT_Get_First_Char(FT_Face face, FT_UInt *fui) {
  return _skFT_Get_First_Char(face, fui);
}

FT_ULong skFT_Get_Next_Char(FT_Face face, FT_ULong ful, FT_UInt * fui) {
  return _skFT_Get_Next_Char(face, ful, fui);
}

FT_Long  skFT_MulFix(FT_Long fla, FT_Long flb) {
  return _skFT_MulFix(fla, flb);
}

FT_Error skFT_Outline_Embolden(FT_Outline *fol, FT_Pos fpos) {
  return _skFT_Outline_Embolden(fol, fpos);
}

void     skFT_Set_Transform(FT_Face face, FT_Matrix *mx, FT_Vector *vec) {
  _skFT_Set_Transform(face, mx, vec);
}

void *   skFT_Get_Sfnt_Table(FT_Face face, FT_Sfnt_Tag fst) {
  return _skFT_Get_Sfnt_Table(face, fst);
}

FT_Error skFT_Outline_Decompose(FT_Outline *fol, const FT_Outline_Funcs *fof, void *ptr) {
  return _skFT_Outline_Decompose(fol, fof, ptr);
}

FT_Error skFT_GlyphSlot_Own_Bitmap(FT_GlyphSlot fgs) {
  return _skFT_GlyphSlot_Own_Bitmap(fgs);
}

FT_Error skFT_Bitmap_Embolden(FT_Library lib, FT_Bitmap *bmp, FT_Pos a, FT_Pos b) {
  return _skFT_Bitmap_Embolden(lib, bmp, a, b);
}

FT_Error skFT_Get_Advance(FT_Face face, FT_UInt fui, FT_Int32 fi, FT_Fixed *ff) {
  return _skFT_Get_Advance(face, fui, fi, ff);
}

FT_Error skFT_Library_SetLcdFilter(FT_Library lib, FT_LcdFilter flf) {
  if(_skFT_Library_SetLcdFilter)
    return _skFT_Library_SetLcdFilter(lib, flf);
  else
    return FT_Err_Ok;
}

FT_Error skFT_New_Size(FT_Face face, FT_Size *size) {
  return _skFT_New_Size(face, size);
}

FT_Error skFT_Activate_Size(FT_Size size) {
  return _skFT_Activate_Size(size);
}

FT_Error skFT_Set_Char_Size(FT_Face face, FT_F26Dot6 a, FT_F26Dot6 b, FT_UInt fuia, FT_UInt fuib) {
  return _skFT_Set_Char_Size(face, a, b, fuia, fuib);
}

FT_Error skFT_Done_Size(FT_Size size) {
  return _skFT_Done_Size(size);
}

void     skFT_Outline_Translate(const FT_Outline *fol, FT_Pos a, FT_Pos b) {
  _skFT_Outline_Translate(fol, a, b);
}

FT_Error skFT_Outline_Get_Bitmap(FT_Library lib, FT_Outline *fol, const FT_Bitmap *bmp) {
  return _skFT_Outline_Get_Bitmap(lib, fol, bmp);
}

FT_Error skFT_Render_Glyph(FT_GlyphSlot fgs, FT_Render_Mode frm) {
  return _skFT_Render_Glyph(fgs, frm);
}

FT_Error skFT_Get_PS_Font_Info(FT_Face face, PS_FontInfo pfi) {
  if(_skFT_Get_PS_Font_Info)
    return _skFT_Get_PS_Font_Info(face, pfi);
  else
    return FT_Err_Ok;
}

FT_Error skFT_Set_Charmap(FT_Face face, FT_CharMap fcm) {
  return _skFT_Set_Charmap(face, fcm);
}

FT_Error skFT_Get_Glyph_Name(FT_Face face, FT_UInt a, FT_Pointer fp, FT_UInt b) {
  return _skFT_Get_Glyph_Name(face, a, fp, b);
}

FT_Error skFT_Get_Advances(FT_Face face, FT_UInt a, FT_UInt b, FT_Int32 c, FT_Fixed *d) {
  return _skFT_Get_Advances(face, a, b, c, d);
}

FT_UShort  skFT_Get_FSType_Flags(FT_Face face) {
  return _skFT_Get_FSType_Flags(face);
}

const char* skFT_Get_X11_Font_Format(FT_Face face) {
  if(_skFT_Get_X11_Font_Format)
    return _skFT_Get_X11_Font_Format(face);
  else
    return FT_Err_Ok;
}

const char* skFT_Get_Postscript_Name(FT_Face face) {
  if(_skFT_Get_Postscript_Name)
    return _skFT_Get_Postscript_Name(face);
  else
    return FT_Err_Ok;
}

void freetype_glue_uninit() {
  if(g_handle) {
    dlclose(g_handle);
  }
}

#ifdef __cplusplus
}
#endif
