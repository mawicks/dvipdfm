/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/jpeg.c,v 1.5 1999/02/09 03:24:07 mwicks Exp $

    This is dvipdf, a DVI to PDF translator.
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
#include "system.h"
#include "mfileio.h"
#include "mem.h"
#include "numbers.h"
#include "jpeg.h"
#include "pdfobj.h"
#include "dvi.h"
#include "pdfspecial.h"

#define verbose 0

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

int check_for_jpeg(FILE *file)
{
  rewind (file);
  if (get_unsigned_byte (file) != 0xff ||
      get_unsigned_byte (file) != SOI)
    return 0;
  return 1;
}

int jpeg_headers (struct jpeg *jpeg) 
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
      if (verbose) {
	fprintf (stderr, "ht=%d,wd=%d,co=%d,bpc=%d\n",
		 jpeg->height,jpeg->width,jpeg->colors,jpeg->bits_per_color);
      }
      
      done = 1;
      return 1;
    default:
      for (i=0; i<length; i++) {
	get_unsigned_byte(jpeg -> file);
      }
    }
  }
  return 0;			/* Not reached */
}

struct jpeg *jpeg_open (char *filename)
{
  struct jpeg *jpeg;
  FILE *file;
  if (verbose) {
    fprintf (stderr, "Opening image file %s\n", filename);
  }
  if ((file = fopen (filename, FOPEN_RBIN_MODE)) == NULL) {
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
    RELEASE (jpeg);
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
  RELEASE (jpeg);
  return;
}


