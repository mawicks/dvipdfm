/*
    This is dvipdf, a DVI to PDF translator.
    Copyright (C) 1998  by Mark A. Wicks

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
#include "io.h"
#include "mem.h"
#include "numbers.h"
#include "jpeg.h"
#include "pdfobj.h"
#include "dvi.h"

#define SOF0	0xc0
#define SOF1	0xc1
#define SOF2	0xc2
#define SOF3	0xc3
#define SOF5	0xc5
#define SOF6	0xc6
#define SOF7	0xc7
#define SOF9	0xc9
#define SOF10	0xca
#define SOF11	0xcb
#define SOF13	0xcd
#define SOF14	0xce
#define SOF15	0xcf
#define SOI	0xd8
#define EOI	0xd9
#define SOS	0xda
#define COM	0xfe

static int check_for_jpeg(FILE *file)
{
  UNSIGNED_BYTE byte;
  if (get_unsigned_byte (file) != 0xff ||
      get_unsigned_byte (file) != SOI)
    return 0;
  return 1;
}

static int jpeg_headers (struct jpeg *jpeg) 
{
  UNSIGNED_BYTE byte;
  UNSIGNED_PAIR length;
  int i, done;
  done = 0;
  while (!done) {
    if ((byte = get_unsigned_byte (jpeg -> file)) != 0xff)
      return 0;
    while ((byte = get_unsigned_byte (jpeg -> file)) == 0xff);
    length = get_unsigned_pair (jpeg -> file);
    length -= 2;
    switch (byte) {
    case SOF0:
    case SOF1:
    case SOF2:
    case SOF3:
    case SOF5:
    case SOF6:
    case SOF7:
    case SOF9:
    case SOF10:
    case SOF11:
    case SOF13:
    case SOF14:
    case SOF15:
      jpeg -> bits_per_color = get_unsigned_byte (jpeg -> file);
      jpeg -> height = get_unsigned_pair (jpeg -> file);
      jpeg -> width = get_unsigned_pair (jpeg -> file);
      jpeg -> colors = get_unsigned_byte (jpeg -> file);
      done = 1;
      return 1;
    default:
      for (i=0; i<length; i++) {
	get_unsigned_byte(jpeg -> file);
      }
    }
  }
}

struct jpeg *jpeg_open (char *filename)
{
  struct jpeg *jpeg;
  FILE *file;
  if ((file = fopen (filename, "r")) == NULL) {
    fprintf (stderr, "\nUnable to open file named %s\n", filename);
    return NULL;
  }
  if (!check_for_jpeg(file)) {
    fprintf (stderr, "\n%s: Not a JPEG file\n", filename);
    fclose (file);
    return NULL;
  }
  jpeg = NEW (1, struct jpeg);
  jpeg -> file = file;
  if (!jpeg_headers(jpeg)) {
    fprintf (stderr, "\n%s: Corrupt JPEG file?\n", filename);
    fclose (file);
    release (jpeg);
    return NULL;
  }
  return jpeg;
}

void jpeg_close (struct jpeg *jpeg)
{
  if (jpeg == NULL) {
    fprintf (stderr, "jpeg_closed: passed invalid pointer\n");
  }
  fclose (jpeg -> file);
  release (jpeg);
  return;
}

static num_images = 0;
pdf_obj *jpeg_build_object(struct jpeg *jpeg, double x_user, double
			   y_user, double width, double height) 
{
  pdf_obj *xobject, *xobj_dict;
  double xscale, yscale;
  xobject = pdf_new_stream();
  sprintf (work_buffer, "Im%d", ++num_images);
  pdf_doc_add_to_page_xobjects (work_buffer, pdf_ref_obj (xobject));
  xobj_dict = pdf_stream_dict (xobject);

  pdf_add_dict (xobj_dict, pdf_new_name ("Name"),
		pdf_new_name (work_buffer));
  pdf_add_dict (xobj_dict, pdf_new_name ("Type"),
		pdf_new_name ("XObject"));
  pdf_add_dict (xobj_dict, pdf_new_name ("Subtype"),
		pdf_new_name ("Image"));
  pdf_add_dict (xobj_dict, pdf_new_name ("Width"),
		pdf_new_number (jpeg -> width));
  pdf_add_dict (xobj_dict, pdf_new_name ("Height"),
		pdf_new_number (jpeg -> height));
  pdf_add_dict (xobj_dict, pdf_new_name ("BitsPerComponent"),
		pdf_new_number (jpeg -> bits_per_color));
  if (jpeg->colors == 1)
    pdf_add_dict (xobj_dict, pdf_new_name ("ColorSpace"),
		  pdf_new_name ("DeviceGray"));
  if (jpeg->colors > 1)
    pdf_add_dict (xobj_dict, pdf_new_name ("ColorSpace"),
		  pdf_new_name ("DeviceRGB"));
  pdf_add_dict (xobj_dict, pdf_new_name ("Filter"),
		pdf_new_name ("DCTDecode"));
  {
    int length;
    rewind (jpeg -> file);
    while ((length = fread (work_buffer, sizeof (char),
			    WORK_BUFFER_SIZE, jpeg -> file)) > 0) {
      pdf_add_stream (xobject, work_buffer, length);
    }
  }
  {
    xscale = jpeg -> width * dvi_tell_mag() * (72.0 / 100.0);
    yscale = jpeg -> height * dvi_tell_mag() * (72.0 / 100.0);
    if (width != 0.0) {
      xscale = width;
      if (height == 0.0)
	yscale = xscale;
    }
    if (height != 0.0) {
      yscale = height;
      if (width = 0.0)
	xscale = yscale;
    }
  }
  
  sprintf (work_buffer, " q %g 0 0 %g  %g %g cm /Im%d Do Q ", xscale,
	   yscale, x_user, y_user, num_images);
  pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
  return (xobject);
}

