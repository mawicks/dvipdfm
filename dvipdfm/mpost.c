/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/mpost.c,v 1.3 1999/08/25 03:52:00 mwicks Exp $

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

#include <stdlib.h>

#include "mfileio.h"
#include "pdfobj.h"
#include "pdfspecial.h"
#include "mpost.h"
#include "string.h"
#include "pdfparse.h"
#include "mem.h"
#include "pdflimits.h"
#include "error.h"
#include "pdfdev.h"

int check_for_mp (FILE *image_file) 
{
  rewind (image_file);
  /* For now, this is an exact test that must be passed, character for
     character */
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strncmp (work_buffer, "%!PS", 4))
    return 0;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strlen(work_buffer)>=strlen("%%BoundingBox") &&
      strncmp (work_buffer, "%%BoundingBox", strlen("%%BoundingBox")))
    return 0;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strlen(work_buffer) >= strlen("%%Creator: MetaPost") &&
      strncmp (work_buffer, "%%Creator: MetaPost", strlen("%%Creator: MetaPost")))
    return 0;
  fprintf (stderr, "\nMetapost file\n");
  return 1;
}
static struct mp_fonts 
{
  char *tex_name;
  int font_id;
  double pt_size;
} mp_fonts[MAX_FONTS];
int n_mp_fonts = 0;

int mp_locate_font (char *tex_name, double pt_size)
{
  int result, i;
  for (i=0; i<n_mp_fonts; i++) {
    if (!strcmp (tex_name, mp_fonts[i].tex_name) &&
	mp_fonts[i].pt_size == pt_size)
      break;
  }
  if (n_mp_fonts >= MAX_FONTS) {
    ERROR ("Too many fonts in mp_locate_font()");
  }
  if (i == n_mp_fonts) {
    n_mp_fonts += 1;
    mp_fonts[i].tex_name = NEW (strlen(tex_name), char);
    strcpy (mp_fonts[i].tex_name, tex_name);
    mp_fonts[i].pt_size = pt_size;
    /* The following line is a bit of a kludge.  MetaPost inclusion
       was an afterthought */
    mp_fonts[i].font_id = dev_locate_font (tex_name,
					   pt_size/dev_dvi2pts());
    result = mp_fonts[i].font_id;
  } else 
    result = -1;
  fprintf (stderr, "mp_locate_font() =%d\n", result);
  return result;
}

static double bbllx, bblly, bburx, bbury;
int mp_parse_headers (FILE *image_file)
{
  char *start, *end, *token, *name;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  /* Presumably already checked file, so press on */
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strncmp (work_buffer, "%%BoundingBox:", strlen("%%BoundingBox")))
    return 0;
  start = work_buffer + strlen("%%BoundingBox:");
  end = start+strlen(start);
  skip_white (&start, end);
  /* Expect 4 numbers or fail */
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bbllx = atof (token);
    RELEASE(token);
  } else
    return 0;
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bblly = atof (token);
    RELEASE(token);
  } else
    return 0;
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bburx = atof (token);
    RELEASE(token);
  } else
    return 0;
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bbury = atof (token);
    RELEASE(token);
  } else
    return 0;
  /* Got four numbers */
  fprintf (stderr, "bbllx=%g,bblly=%g,bburx=%g,bbury=%g\n",
	   bbllx,bblly,bburx,bbury);
  /* Read all headers--act on *Font records */
  while (!feof(image_file) && mfgets (work_buffer, WORK_BUFFER_SIZE,
				     image_file)) {
    if (*work_buffer != '%')
      break;
    if (*(work_buffer+1) == '*' &&
	!strncmp (work_buffer+2, "Font:", strlen("Font:"))) {
      double ps_ptsize;
      start = work_buffer+strlen("%*Font:");
      end = start+strlen(start);
      skip_white (&start, end);
      if ((name = parse_ident (&start, end))) {
	skip_white (&start, end);
      } else
	return 0;
      if ((token = parse_number (&start, end))) {
	ps_ptsize = atof (token);
	RELEASE (token);
      } else
	return 0;
      fprintf (stderr, "name=%s,size=%g\n", name, ps_ptsize);
      mp_locate_font (name, ps_ptsize);
    }
  }
  return 1;
}

pdf_obj *mp_include (FILE *image_file,  struct xform_info *p,
		     char *res_name) 
{
  rewind (image_file);
  if (mp_parse_headers (image_file)) {
  }
  return NULL;
}



