/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/thumbnail.c,v 1.4 1999/08/13 14:14:38 mwicks Exp $

    This is dvipdfm, a DVI to PDF translator.
    Copyright (C) 1998, 1999 by Mark A. Wicks

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
    
    The author may be contacted via the e-mail address

	mwicks@kettering.edu
*/

#include <stdio.h>
#include <stdlib.h>
#include <kpathsea/c-ctype.h>
#include "config.h"

#include "system.h"
#include "mem.h"
#include "pdfobj.h"
#include "thumbnail.h"

#ifdef HAVE_LIBPNG
#include <png.h>

#define TMP "/tmp"

static char *guess_name (const char *thumb_filename)
{
  /* Build path name for anticipated thumbnail image */
  char *tmpdir, *tmpname;
  fprintf (stderr, "do_thumbnail: %s\n", thumb_filename);
  if (!(tmpdir = getenv ("TMP")) &&
      !(tmpdir = getenv ("TEMP"))) 
    tmpdir = TMP;
  tmpname = NEW (strlen(tmpdir)+strlen(thumb_filename)+strlen(DIR_SEP_STRING)+1,
		 char);
  strcpy (tmpname, tmpdir);
  if (!IS_DIR_SEP (tmpname[strlen(tmpname)-1])) {
    strcat (tmpname, DIR_SEP_STRING);
  }
  strcat (tmpname, thumb_filename);
  return tmpname;
}


pdf_obj *do_png (FILE *file)
{
  pdf_obj *result = NULL, *dict = NULL;
  png_structp png_ptr;
  png_infop info_ptr;
  unsigned long width, height;
  unsigned bit_depth, color_type, interlace_type;
  
  if (!(png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,    
					  NULL, NULL, NULL)) ||
      !(info_ptr = png_create_info_struct (png_ptr))) {
    fprintf (stderr, "\n\nLibpng failed to initialize\n");
    if (png_ptr)
      png_destroy_read_struct(&png_ptr, NULL, NULL);
    return NULL;
  }
  png_init_io (png_ptr, file);
  /*  png_set_sig_bytes (png_ptr, 0); */
  /* Read PNG header */
  png_read_info (png_ptr, info_ptr);
  {
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    interlace_type = png_get_interlace_type(png_ptr, info_ptr);
    fprintf (stderr,
	     "w=%ld,h=%ld,c=%d,b=%d\n\n",width,height,color_type,bit_depth);
    if (color_type != PNG_COLOR_TYPE_PALETTE &&
	color_type != PNG_COLOR_TYPE_RGB) {
      fprintf (stderr, "\n\nExpecting color thumbnails\n");
      return NULL;
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
      png_set_expand(png_ptr);
    }
    if (bit_depth == 16) {
      png_set_strip_16 (png_ptr);
    }
    png_read_update_info(png_ptr, info_ptr);
  }
  { /* Read the image in raw RGB format */
    int i, rowbytes;
    png_bytep *rows;
    rows = NEW (height, png_bytep);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    fprintf (stderr, "rowbytes=%d\n", rowbytes);
    for (i=0; i<height; i++) {
      rows[i] = NEW (rowbytes, png_byte);
    }
    png_read_image(png_ptr, rows);
    result = pdf_new_stream(STREAM_COMPRESS);
    dict = pdf_stream_dict(result);
    pdf_add_dict (dict, pdf_new_name ("Width"),
		  pdf_new_number(width));
    pdf_add_dict (dict, pdf_new_name ("Height"),
		  pdf_new_number(height));
    pdf_add_dict (dict, pdf_new_name ("BitsPerComponent"),
		  pdf_new_number(8.0));
    pdf_add_dict (dict, pdf_new_name ("ColorSpace"),
		  pdf_new_name ("DeviceRGB"));
    for (i=0; i<height; i++) {
      pdf_add_stream (result, (char *) rows[i], rowbytes);
      RELEASE (rows[i]);
    }
    RELEASE (rows);
  }
  { /* Cleanup  */
    if (png_ptr)
      png_destroy_read_struct(&png_ptr, NULL, NULL);
  }
  return result;
}


static unsigned char sigbytes[4];

pdf_obj *do_thumbnail (const char *thumb_filename) 
{
  pdf_obj *image_stream = NULL, *image_ref = NULL;
  FILE *thumb_file;
  char *filename;
  filename = guess_name (thumb_filename);
  fprintf (stderr, "%s\n", filename);
  fprintf (stderr, "%s\n", thumb_filename);
  if (!(thumb_file = fopen (thumb_filename, FOPEN_RBIN_MODE)) &&
      !(thumb_file = fopen (filename, FOPEN_RBIN_MODE))) {
    fprintf (stderr, "\nNo thumbnail file\n");
    return NULL;
  }
  else 
    fprintf (stderr, "FOUND!\n");
  RELEASE (filename);
  if (fread (sigbytes, 1, sizeof(sigbytes), thumb_file) !=
      sizeof(sigbytes) ||
      (!png_check_sig (sigbytes, sizeof(sigbytes)))) {
    fprintf (stderr, "\nNot a png file! Skipping\n");
    return NULL;
  }
  rewind (thumb_file);
  fprintf (stderr, "Looks good...\n");
  if ((image_stream = do_png (thumb_file))) {
    image_ref = pdf_ref_obj (image_stream);
    pdf_release_obj (image_stream);
  } else {
    image_ref = NULL;
  }
  return image_ref;
}

#endif /* HAVE_LIBPNG */

