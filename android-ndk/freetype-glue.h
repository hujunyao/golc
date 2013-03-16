#ifndef __FREETYPE_GLUE_H__
#define __FREETYPE_GLUE_H__
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/tttables.h>
#include <freetype/t1tables.h>
#include <freetype/ftlcdfil.h>

#undef FT_LCD_FILTER_H

#ifdef __cplusplus
#define FG_EXPORT extern "C"
#else
#define FG_EXPORT extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

FG_EXPORT FT_Error skFT_Init_FreeType(FT_Library *);
FG_EXPORT FT_Error skFT_Open_Face(FT_Library, const FT_Open_Args*, FT_Long, FT_Face *);
FG_EXPORT FT_Error skFT_Done_Face(FT_Face face);
FG_EXPORT FT_Error skFT_Done_FreeType(FT_Library );
FG_EXPORT FT_UInt  skFT_Get_Char_Index(FT_Face , FT_ULong );
FG_EXPORT FT_Error skFT_Load_Glyph(FT_Face , FT_UInt, FT_Int32);
FG_EXPORT void     skFT_Outline_Get_CBox(const FT_Outline*, FT_BBox *);
FG_EXPORT FT_ULong skFT_Get_First_Char(FT_Face , FT_UInt *);
FG_EXPORT FT_ULong skFT_Get_Next_Char(FT_Face , FT_ULong , FT_UInt *);
FG_EXPORT FT_Long  skFT_MulFix(FT_Long, FT_Long);
FG_EXPORT FT_Error skFT_Outline_Embolden(FT_Outline *, FT_Pos);
FG_EXPORT void     skFT_Set_Transform(FT_Face, FT_Matrix *, FT_Vector *);
FG_EXPORT void *   skFT_Get_Sfnt_Table(FT_Face, FT_Sfnt_Tag );
FG_EXPORT FT_Error skFT_Outline_Decompose(FT_Outline *, const FT_Outline_Funcs *, void *);
FG_EXPORT FT_Error skFT_GlyphSlot_Own_Bitmap(FT_GlyphSlot );
FG_EXPORT FT_Error skFT_Bitmap_Embolden(FT_Library , FT_Bitmap *, FT_Pos, FT_Pos);
FG_EXPORT FT_Error skFT_Get_Advance(FT_Face , FT_UInt, FT_Int32, FT_Fixed *);
FG_EXPORT FT_Error skFT_Library_SetLcdFilter(FT_Library , FT_LcdFilter);
FG_EXPORT FT_Error skFT_New_Size(FT_Face , FT_Size *);
FG_EXPORT FT_Error skFT_Activate_Size(FT_Size );
FG_EXPORT FT_Error skFT_Set_Char_Size(FT_Face, FT_F26Dot6, FT_F26Dot6, FT_UInt, FT_UInt);
FG_EXPORT FT_Error skFT_Done_Size(FT_Size );
FG_EXPORT void     skFT_Outline_Translate(const FT_Outline *, FT_Pos, FT_Pos);
FG_EXPORT FT_Error skFT_Outline_Get_Bitmap(FT_Library , FT_Outline *, const FT_Bitmap *);
FG_EXPORT FT_Error skFT_Render_Glyph(FT_GlyphSlot , FT_Render_Mode);
FG_EXPORT FT_Error skFT_Get_PS_Font_Info(FT_Face , PS_FontInfo);
FG_EXPORT FT_Error skFT_Set_Charmap(FT_Face , FT_CharMap);
FG_EXPORT FT_Error skFT_Get_Glyph_Name(FT_Face, FT_UInt, FT_Pointer, FT_UInt);
FG_EXPORT FT_Error skFT_Get_Advances(FT_Face , FT_UInt, FT_UInt, FT_Int32, FT_Fixed *);
FG_EXPORT FT_UShort  skFT_Get_FSType_Flags(FT_Face );
FG_EXPORT const char* skFT_Get_X11_Font_Format(FT_Face );
FG_EXPORT const char* skFT_Get_Postscript_Name(FT_Face );

FG_EXPORT int  freetype_glue_init();
FG_EXPORT void freetype_glue_uninit();

//FG_EXPORT skFT_Init_FreeType_t skFT_Init_FreeType;
//FG_EXPORT skFT_Open_Face_t skFT_Open_Face;
//FG_EXPORT skFT_Done_Face_t skFT_Done_Face;
//FG_EXPORT skFT_Done_FreeType_t skFT_Done_FreeType;
//FG_EXPORT skFT_Get_Char_Index_t skFT_Get_Char_Index;
//FG_EXPORT skFT_Load_Glyph_t skFT_Load_Glyph;
//FG_EXPORT skFT_Outline_Get_CBox_t skFT_Outline_Get_CBox;
//FG_EXPORT skFT_Get_First_Char_t skFT_Get_First_Char;
//FG_EXPORT skFT_Get_Next_Char_t skFT_Get_Next_Char;
//FG_EXPORT skFT_MulFix_t skFT_MulFix;
//FG_EXPORT skFT_Outline_Embolden_t skFT_Outline_Embolden;
//FG_EXPORT skFT_Set_Transform_t skFT_Set_Transform;
//FG_EXPORT skFT_Get_Sfnt_Table_t skFT_Get_Sfnt_Table;
//FG_EXPORT skFT_Outline_Decompose_t skFT_Outline_Decompose;
//FG_EXPORT skFT_GlyphSlot_Own_Bitmap_t skFT_GlyphSlot_Own_Bitmap;
//FG_EXPORT skFT_Bitmap_Embolden_t skFT_Bitmap_Embolden;
//FG_EXPORT skFT_Get_Advance_t skFT_Get_Advance;
//FG_EXPORT skFT_Library_SetLcdFilter_t skFT_Library_SetLcdFilter;
//FG_EXPORT skFT_New_Size_t skFT_New_Size;
//FG_EXPORT skFT_Activate_Size_t skFT_Activate_Size;
//FG_EXPORT skFT_Set_Char_Size_t skFT_Set_Char_Size;
//FG_EXPORT skFT_Done_Size_t skFT_Done_Size;
//FG_EXPORT skFT_Outline_Translate_t skFT_Outline_Translate;
//FG_EXPORT skFT_Outline_Get_Bitmap_t skFT_Outline_Get_Bitmap;
//FG_EXPORT skFT_Render_Glyph_t skFT_Render_Glyph;
//FG_EXPORT skFT_Get_Postscript_Name_t skFT_Get_Postscript_Name;
//FG_EXPORT skFT_Get_PS_Font_Info_t skFT_Get_PS_Font_Info;
//FG_EXPORT skFT_Set_Charmap_t skFT_Set_Charmap;
//FG_EXPORT skFT_Get_Glyph_Name_t skFT_Get_Glyph_Name;
//FG_EXPORT skFT_Get_FSType_Flags_t skFT_Get_FSType_Flags;
//FG_EXPORT skFT_Get_X11_Font_Format_t skFT_Get_X11_Font_Format;
//FG_EXPORT skFT_Get_Advances_t skFT_Get_Advances;

#ifdef __cplusplus
}
#endif

#endif
