//	flib - A TrueType font library for the PSP
//
//	Based on the freetype2 tutorials and example code
//
//	Copyright 2006 samedi1971@yahoo.com
//
//	Requirements:	PSP graphics library by Psylobeing
//				libpng for PSP
//				libfreetype2 for PSP

// TODO:	Need to remove library load/unload from load_font() and
//		unload_font() and create new init() and destroy()
//		functions instead.
//
//		Add flib_ prefix to public function names.
//
//		Add glyph/bitmap caches.
//
//		Add public cache control functions.
//
//		Move angle transform code to set_font_angle().
//
//		Add support for multiple faces.

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "graphics.h"
#include "flib.h"

#include <ft2build.h>
#include FT_FREETYPE_H


// private
Color font_color = 0xffffffff;
double font_angle = 0.0;
FT_Library library = 0;
FT_Face face = 0;

//private
void render_font(char *text, Image *image, int x, int y);
void draw_bitmap(Image *image, FT_Bitmap *bitmap, FT_Int x, FT_Int y, int width, int height);


void set_font_color(Color color)
{
	font_color = color;
}


void set_font_angle(double angle)
{
	if(angle >= 360.0 || angle <= -360.0)
		font_angle = 0.0;
	else if(angle < 0.0)
		font_angle = 360.0 - angle;
	else
		font_angle = angle;
}


int set_font_size(int size)
{
	FT_Error error;

	if(!face)
		return FLIB_ERR_INVALIDFONT;

	error = FT_Set_Char_Size( face, size * 64, 0, 100, 0 );
	if(error)
		return FLIB_ERR_BADSIZE;

	return 0;
}


// Note:	load_font() returns a void pointer which will be defined in
//		a future flib version when support for multiple fonts is added.
//		At that time a set_font() function will be added to switch the
//		current font face used in all other functions.  For now just
//		treat the return value as NULL on failure and non-NULL on
//		success.
FLIB_FONT load_font(char *filename)
{
	FT_Error error;

	error = FT_Init_FreeType( &library );              /* initialize library */
  	if(error)
		return 0;

  	error = FT_New_Face( library, filename, 0, &face );
	if(error)
		return 0;

	FT_Set_Char_Size( face, 14 * 64, 0, 100, 0 );	// set default 14 point size

	return (FLIB_FONT) 1;
}


void unload_font(void)
{
	FT_Done_Face(face);
	FT_Done_FreeType(library);
	face = 0;
	library = 0;
}


void text_to_screen(char *text, int x, int y)
{
	render_font(text, 0, x, y);
}


void text_to_image(char *text, Image *image, int x, int y)
{
	render_font(text, image, x, y);
}


void render_font(char *text, Image *image, int x, int y)
{
	FT_GlyphSlot slot;
	FT_Matrix matrix;
	FT_Vector pen;
	FT_Error error;
	double angle;
	int target_height = 272;
	int n, num_chars;

	if(!text || !*text)
		return;

	if(image)
		target_height = image->textureHeight;

	slot = face->glyph;
	num_chars = strlen(text);

	angle = ( font_angle / 360 ) * 3.14159 * 2;
	matrix.xx = (FT_Fixed)(cos(angle) * 0x10000L);
	matrix.xy = (FT_Fixed)(-sin(angle) * 0x10000L);
	matrix.yx = (FT_Fixed)(sin(angle) * 0x10000L);
	matrix.yy = (FT_Fixed)(cos(angle) * 0x10000L);

	pen.x = x * 64;
	pen.y = (target_height - y) * 64;

	for(n = 0; n < num_chars; n++)
	{
		FT_Set_Transform(face, &matrix, &pen);

		error = FT_Load_Char( face, text[n], FT_LOAD_RENDER );
		if(error)
			continue;                 /* ignore errors */

		if(image)
			draw_bitmap(image, &slot->bitmap, slot->bitmap_left,
				target_height - slot->bitmap_top, image->imageWidth,
				image->imageHeight);
		else // to screen
			draw_bitmap(0, &slot->bitmap, slot->bitmap_left,
				target_height - slot->bitmap_top, 480,
				272);

		pen.x += slot->advance.x;
		pen.y += slot->advance.y;
	}
}


/*void draw_bitmap(Image *image, FT_Bitmap *bitmap, FT_Int x, FT_Int y, int width, int height)
{
	FT_Int i, j, p, q;
	FT_Int x_max = x + bitmap->width;
	FT_Int y_max = y + bitmap->rows;
	Color pixel, grey;
	int r, g, b;

	for(i = x, p = 0; i < x_max; i++, p++)
	{
		for(j = y, q = 0; j < y_max; j++, q++)
		{
			if(i >= width || i < 0 || j >= height || j < 0)
				continue;

			grey = bitmap->buffer[q * bitmap->width + p];

			if(grey > 0)
			{
				r = (grey * R(font_color)) / 255;
				g = (grey * G(font_color)) / 255;
				b = (grey * B(font_color)) / 255;
	  			pixel = 0xff000000 | (b << 16) | (g << 8) | r;
			}
			else
	  			pixel = 0;

			if(pixel && image)
				putPixelImage(pixel, i, j, image);
			else if(pixel)
	  			putPixelScreen(pixel, i, j);
		}
	}
}*/


/*void draw_bitmap(Image *image, FT_Bitmap *bitmap, FT_Int x, FT_Int y, int width, int height)
{
	FT_Int i, j, p, q;
	FT_Int x_max = x + bitmap->width;
	FT_Int y_max = y + bitmap->rows;
	Color pixel, grey, dpixel;
//	int r, g, b, a;
	int fr, fg, fb, fa;	// font pixel colors
	int dr, dg, db, da;	// destination pixel colors
	int nr, ng, nb, na;	// new pixel colors
	int fca;		// font combined alpha

	fr = R(font_color);
	fg = G(font_color);
	fb = B(font_color);
	fa = A(font_color);

	for(i = x, p = 0; i < x_max; i++, p++)
	{
		for(j = y, q = 0; j < y_max; j++, q++)
		{
			if(i >= width || i < 0 || j >= height || j < 0)
				continue;

			grey = bitmap->buffer[q * bitmap->width + p];

			if(grey > 0)
			{
				if(image)
					dpixel = getPixelImage(i, j, image);
				else
					dpixel = getPixelScreen(i, j);
				da = A(dpixel);
				dr = (R(dpixel) * da) / 255;
				dg = (G(dpixel) * da) / 255;
				db = (B(dpixel) * da) / 255;

				fca = (fa * grey) / 255;

				nr = dr - (((dr - fr) * fca) / 255);
				ng = dg - (((dg - fg) * fca) / 255);
				nb = db - (((db - fb) * fca) / 255);
				if(fa > da)
					na = fa;
				else
					na = da;

				if(nr < 0 || nr > 255 || ng < 0 || ng > 255 || nb < 0 || nb > 255)
				{
					nr = 255;
					ng = 255;
					ng = 0;
				}

//				r = (grey * R(font_color)) / 255;
//				g = (grey * G(font_color)) / 255;
//				b = (grey * B(font_color)) / 255;
	  			pixel = (na << 24) | (nb << 16) | (ng << 8) | nr;
			}
			else
	  			pixel = 0;

			if(pixel && image)
				putPixelImage(pixel, i, j, image);
			else if(pixel)
	  			putPixelScreen(pixel, i, j);
		}
	}
}*/


void draw_bitmap(Image *image, FT_Bitmap *bitmap, FT_Int x, FT_Int y, int width, int height)
{
	FT_Int i, j, p, q;
	FT_Int x_max = x + bitmap->width;
	FT_Int y_max = y + bitmap->rows;
	Color pixel, grey, dpixel;
//	int r, g, b, a;
	int fr, fg, fb, fa;	// font pixel colors
	int dr, dg, db, da;	// destination pixel colors
	int nr, ng, nb, na;	// new pixel colors
	int fca;			// font combined alpha

	fr = R(font_color);
	fg = G(font_color);
	fb = B(font_color);
	fa = A(font_color);

	for(i = x, p = 0; i < x_max; i++, p++)
	{
		for(j = y, q = 0; j < y_max; j++, q++)
		{
			if(i >= width || i < 0 || j >= height || j < 0)
				continue;

			grey = bitmap->buffer[q * bitmap->width + p];

			if(grey > 0)
			{
				if(image)
					dpixel = getPixelImage(i, j, image);
				else
					dpixel = getPixelScreen(i, j);
				da = A(dpixel);
				dr = R(dpixel);
				dg = G(dpixel);
				db = B(dpixel);

				fca = (fa * grey) / 255;

				nr = dr - (((dr - fr) * fca) / 255);
				ng = dg - (((dg - fg) * fca) / 255);
				nb = db - (((db - fb) * fca) / 255);
				if(fa > da)
					na = fa;
				else
					na = da;

				if(nr < 0 || nr > 255 || ng < 0 || ng > 255 || nb < 0 || nb > 255)
				{
					nr = 255;
					ng = 255;
					ng = 0;
				}

//				r = (grey * R(font_color)) / 255;
//				g = (grey * G(font_color)) / 255;
//				b = (grey * B(font_color)) / 255;
	  			pixel = (na << 24) | (nb << 16) | (ng << 8) | nr;
			}
			else
	  			pixel = 0;

			if(pixel && image)
				putPixelImage(pixel, i, j, image);
			else if(pixel)
	  			putPixelScreen(pixel, i, j);
		}
	}
}

