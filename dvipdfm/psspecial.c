/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/psspecial.c,v 1.1 1999/09/05 13:16:43 mwicks Exp $
    
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
#include "system.h"
#include "mem.h"
#include "mfileio.h"
#include "psspecial.h"
#include "pdfparse.h"
#include "pdfspecial.h"
#include "psimage.h"
#include "pdfdoc.h"

#define HOFFSET 1
#define VOFFSET 2
#define HSIZE   3
#define VSIZE   4
#define HSCALE  5
#define VSCALE  6
#define ANGLE   7
#define CLIP    8
#define LLX     9
#define LLY    10
#define URX    11
#define URY    12
#define RWI    13
#define RHI    14

struct keys
{
  char *key;
  int val;
} keys[] = {
  {"hoffset", HOFFSET},
  {"voffset", VOFFSET},
  {"hsize", HSIZE},
  {"vsize", VSIZE},
  {"hscale", HSCALE},
  {"vscale", VSCALE},
  {"angle", ANGLE},
  {"clip", CLIP},
  {"llx", LLX},
  {"lly", LLY},
  {"urx", URX},
  {"ury", URY},
  {"rwi", RWI},
  {"rhi", RHI}
};

static unsigned long next_image = 1;

static int parse_psfile (char **start, char *end, double x_user, double y_user) 
{
  char *key, *val, *filename;
  FILE *image_file;
  double hoffset = 0.0, voffset = 0.0;
  double hsize = 0.0, vsize = 0.0;
  pdf_obj *result = NULL;
  int error = 0;
  struct xform_info *p = new_xform_info();
  parse_key_val (start, end, &key, &val);
  if (key && val) {
    filename = val;
    RELEASE (key);
    skip_white (start, end);
    while (*start < end) {
      parse_key_val (start, end, &key, &val);
      if (key && val) {
	int i;
	for (i=0; i<sizeof(keys)/sizeof(keys[0]); i++) {
	  if (!strcmp (key, keys[i].key))
	    break;
	}
	if (i == sizeof(keys)/sizeof(keys[0])) {
	  fprintf (stderr, "\nUnknown key in special: %s\n", key);
	  break;
	}
	switch (keys[i].val) {
	case HOFFSET:
	  hoffset = atof (val);
	  break;
	case VOFFSET:
	  voffset = atof (val);
	  break;
	case HSIZE:
	  hsize = atof (val);
	  break;
	case VSIZE:
	  vsize = atof (val);
	  break;
	case HSCALE:
	  p -> xscale = atof(val)/100.0;
	  break;
	case VSCALE:
	  p -> yscale = atof(val)/100.0;
	  break;
	case ANGLE:
	  p -> rotate = atof(val)/100.0;
	  break;
	case CLIP:
	  break;
	case LLX:
	  p -> user_bbox = 1;
	  p -> llx = atof(val);
	  break;
	case LLY:
	  p -> user_bbox = 1;
	  p -> lly = atof(val);
	  break;
	case URX:
	  p -> user_bbox = 1;
	  p -> urx = atof(val);
	  break;
	case URY:
	  p -> user_bbox = 1;
	  p -> ury = atof(val);
	  break;
	case RWI:
	  p -> width = atof(val)/10.0;
	  break;
	case RHI:
	  p -> height = atof(val)/10.0;
	  break;
	}
	RELEASE (val);
      }
      skip_white (start, end);
      if (key)
	RELEASE (key);
    } /* If here and *start == end we got something */
    fprintf (stderr,
	     "hsize=%g,vsize=%g,hoffset=%g,voffset=%g,hscale=%g,vscale=%g,angle=%g\n",
	     hsize, vsize, hoffset, voffset, p->xscale, p->yscale, p->rotate);
    fprintf (stderr,
	     "llx=%g,lly=%g,urx=%g,ury=%g\n",
	     p->llx,p->lly,p->urx,p->ury);
    fprintf (stderr,
	     "width=%g,height=%g\n",
	     p->width, p->height);
    if (*start == end && validate_image_xform_info (p)) {
      static char res_name[16];
      char *kpse_file_name;
      sprintf (res_name, "Ps%ld", next_image);
      if ((kpse_file_name = kpse_find_pict (filename)) &&
	  (image_file = FOPEN (kpse_file_name, FOPEN_RBIN_MODE)) &&
	  check_for_ps (image_file)) {
	fprintf (stderr, "(%s", kpse_file_name);
	result = ps_include (kpse_file_name, p, res_name, x_user, 
			     y_user);
	FCLOSE (image_file);
	fprintf (stderr, ")");
      }
      if (result) {
	int len;
	next_image += 1;
	pdf_doc_add_to_page_xobjects (res_name, pdf_ref_obj (result));
	pdf_release_obj (result);
	pdf_doc_add_to_page (" q", 2);
	add_xform_matrix (x_user, y_user, p->xscale, p->yscale,
			  p->rotate);
	len = sprintf (work_buffer, " /%s Do Q", res_name);
	pdf_doc_add_to_page (work_buffer, len);
      }
    }
  } else {
    fprintf (stderr, "\nError parsing PSfile=...\n");
    error = 1;
  }
  RELEASE (p);
  fprintf (stderr, "error=%d\n", error);
  return !error;
}

int ps_parse_special (char *buffer, UNSIGNED_QUAD size, double x_user,
		      double y_user)
{
  char *start = buffer, *end;
  int result = 0;
  end = buffer + size;
  skip_white (&start, end);
  if (!strncmp (start, "PSfile", strlen("PSfile")) ||
      !strncmp (start, "psfile", strlen("PSfile"))) {
    if (parse_psfile(&start, end, x_user, y_user)) {
      result = 1;
    }
  } else if (!strncmp (start, "ps:", strlen("ps:")) ||
	     !strncmp (start, "PS:", strlen("PS:"))) {
    start += 3;
    if (do_raw_ps_special (&start, end)) 
      result = 1;
  }
  return result;
}







