/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/psspecial.c,v 1.6 1999/09/08 16:51:48 mwicks Exp $
    
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
#include "mpost.h"
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
  int id;
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
  {"rhi", RHI},
};

static int parse_psfile (char **start, char *end, double x_user, double y_user) 
{
  char *key, *val, *filename = NULL;
  double hoffset = 0.0, voffset = 0.0;
  double hsize = 0.0, vsize = 0.0;
  int error = 0;
  struct xform_info *p = new_xform_info();
  parse_key_val (start, end, &key, &val);
  if (key && val) {
    filename = val;
    RELEASE (key);
    skip_white (start, end);
    while (*start < end) {
      parse_key_val (start, end, &key, &val);
      if (key) {
	int i;
	for (i=0; i<sizeof(keys)/sizeof(keys[0]); i++) {
	  if (!strcmp (key, keys[i].key))
	    break;
	}
	if (i == sizeof(keys)/sizeof(keys[0])) {
	  fprintf (stderr, "\nUnknown key in special: %s\n", key);
	  break;
	}
	if (val) {
	  switch (keys[i].id) {
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
	  default:
	    fprintf (stderr, "\nPSfile key \"%s=%s\" not recognized",
		     key, val);
	    error = 1;
	  }
	  RELEASE (val);
	} else {  /* Keywords without values */
	  switch (keys[i].id) {
	  case CLIP: /* Ignore */
	    break;
	  default:
	    fprintf (stderr, "\nPSfile key \"%s\" not recognized\n",
		     key);
	    error = 1;
	  }
	}
	RELEASE (key);
      } else {
	fprintf (stderr, "\nError parsing PSfile special\n");
      }
      skip_white (start, end);
    } /* If here and *start == end we got something */
    if (*start == end && validate_image_xform_info (p)) {
      pdf_obj *result;
      result = embed_image (filename, p, x_user, y_user, NULL);
      if (result)
	pdf_release_obj (result);
    }
  } else {
    fprintf (stderr, "\nPSfile special has no filename\n");
    error = 1;
  }
  if (filename)
    RELEASE (filename);
  release_xform_info (p);
  return !error;
}

static void do_texfig (char **start, char *end, double x_user, double y_user)
{
  char *filename;
  struct xform_info *p;
  if (*start < end && (filename = parse_ident (start, end))) {

    p = texfig_info ();
    if (validate_image_xform_info (p)) {
      pdf_obj *result;
      result = embed_image (filename, p, x_user, y_user-p->height, NULL);
      if (result)
	pdf_release_obj (result);
    }
    release_xform_info (p);
    RELEASE (filename);
  } else {
    fprintf (stderr, "Expecting filename here:\n");
    dump (*start, end);
  }
}


int ps_parse_special (char *buffer, UNSIGNED_QUAD size, double x_user,
		      double y_user)
{
  char *start = buffer, *end;
  static char cleanup = 1;
  int result = 0;
  end = buffer + size;
  skip_white (&start, end);
  if (!strncmp (start, "PSfile", strlen("PSfile")) ||
      !strncmp (start, "psfile", strlen("PSfile"))) {
    result = 1; /* This means it is a PSfile special, not that it was
		   successful */
    parse_psfile(&start, end, x_user, y_user);
  } else if (!strncmp (start, "ps::[begin]", strlen("ps::[begin]"))) {
    start += strlen("ps::[begin]");
    result = 1; /* Likewise */
    cleanup = 0;
    do_raw_ps_special (&start, end, cleanup);
  } else if (!strncmp (start, "ps::[end]", strlen("ps::[end]"))) {
    start += strlen("ps::[end]");
    result = 1; /* Likewise */
    cleanup = 1;
    do_raw_ps_special (&start, end, cleanup);
  } else if (!strncmp (start, "ps: plotfile", strlen("ps: plotfile"))) {
    /* This is a bizarre, ugly  special case.. Not really postscript
       code */
    start += strlen ("ps: plotfile");
    result = 1;
    do_texfig (&start, end, x_user, y_user);
  } else if (!strncmp (start, "ps:", strlen("ps:")) ||
	     !strncmp (start, "PS:", strlen("PS:"))) {
    start += 3;
    result = 1; /* Likewise */
    do_raw_ps_special (&start, end, 0);
  }
  return result;
}
