/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfspecial.c,v 1.19 1998/12/05 16:51:17 mwicks Exp $

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
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "system.h"
#include "pdflimits.h"
#include "pdfspecial.h"
#include "pdfobj.h"
#include "pdfdoc.h"
#include "pdfdev.h"
#include "pdfparse.h"
#include "numbers.h"
#include "mem.h"
#include "dvi.h"
#include "mfileio.h"
#include "jpeg.h"
#include "epdf.h"

#define verbose 0
#define debug 0

static void add_reference (char *name, pdf_obj *object, char *res_name);
static void release_reference (char *name);
static pdf_obj *lookup_reference(char *name);
static char *lookup_ref_res_name (char *name);
static pdf_obj *lookup_object(char *name);
static void do_content ();
static void do_epdf();
static void do_image();
static pdf_obj *jpeg_build_object(struct jpeg *jpeg,
			   double x_user, double y_user,
			   struct xform_info *p);
static void do_bxobj (char **start, char *end,
		      double x_user, double y_user);
static void do_exobj (void);
static void do_uxobj (char **start, char *end, double x_user, double y_user);

static void do_bop(char **start, char *end)
{
#ifdef MEM_DEBUG
MEM_START
#endif
  if (*start != end)
    pdf_doc_bop (*start, end - *start);
#ifdef MEM_DEBUG
MEM_END
#endif
  return;
}

static void do_eop(char **start, char *end)
{
#ifdef MEM_DEBUG
MEM_START
#endif
  if (*start != end)
    pdf_doc_eop (*start, end - *start);
#ifdef MEM_DEBUG
MEM_END
#endif
}

pdf_obj *get_reference(char **start, char *end)
{
  char *name, *save = *start;
  pdf_obj *result;
  if ((name = parse_pdf_reference(start, end)) != NULL) {
    result = lookup_reference (name);
    if (result == NULL) {
      fprintf (stderr, "\nNamed reference (@%s) doesn't exist.\n", name);
      *start = save;
      dump(*start, end);
    }
    RELEASE (name);
    return result;
  }
  return NULL;
}

pdf_obj *get_reference_lvalue(char **start, char *end)
{
  char *name, *save = *start;
  pdf_obj *result;
  if ((name = parse_pdf_reference(start, end)) != NULL) {
    result = lookup_object (name);
    if (result == NULL) {
      fprintf (stderr, "\nNamed object (@%s) doesn't exist.\n", name);
      *start = save;
      dump(*start, end);
    }
    RELEASE (name);
    return result;
  }
  return NULL;
}


static void do_put(char **start, char *end)
{
  pdf_obj *result, *data;
  skip_white(start, end);
  if ((result = get_reference_lvalue(start, end)) == NULL) {
    fprintf (stderr, "\nSpecial put:  Nonexistent object reference\n");
    return;
  }
  if (result -> type == PDF_DICT) {
    skip_white (start, end);
    if ((data = parse_pdf_dict (start, end)) == NULL) {
      return;
    }
    pdf_merge_dict (result, data);
    parse_crap(start, end);
    return;
  }
  if (result -> type == PDF_ARRAY) {
    skip_white(start, end);
    while (*start < end && 
	   (data = parse_pdf_object (start, end)) != NULL) {
      pdf_add_array (result, data);
      skip_white(start, end);
    }
    if (*start < end) {
      fprintf (stderr, "\nSpecial: put: invalid object.  Rest of command ignored");
    }
    return;
  }
  else {
    fprintf (stderr, "\nSpecial put:  Invalid destination object type\n");
    return;
  }
}

/* The following must be consecutive and
   starting at 0.  Order must match
   dimensions array below */

#define WIDTH 0
#define HEIGHT 1
#define DEPTH 2
#define SCALE 3
#define XSCALE 4
#define YSCALE 5
#define ROTATE 6

struct {
  char *s;
  int key;
  int hasdimension;
} dimensions[] = {
  {"width", WIDTH, 1},
  {"height", HEIGHT, 1},
  {"depth", DEPTH, 1},
  {"scale", SCALE, 0},
  {"xscale", XSCALE, 0},
  {"yscale", YSCALE, 0},
  {"rotate", ROTATE, 0}
};

static struct xform_info *new_xform_info (void)
{
  struct xform_info *result;
  result = NEW (1, struct xform_info);
  result -> width = 0.0;
  result -> height = 0.0;
  result -> depth = 0.0;
  result -> scale = 0.0;
  result -> xscale = 0.0;
  result -> yscale = 0.0;
  result -> rotate = 0.0;
  return result;
}

static void release_xform_info (struct xform_info *p)
{
  RELEASE (p);
  return;
}

static int validate_image_xform_info (struct xform_info *p)
{
  if (p->width != 0.0)
    if (p->scale !=0.0 || p->xscale != 0.0) {
      fprintf (stderr, "\nCan't supply both width and scale\n");
      return 0;
    }
  if (p->height != 0.0) 
    if (p->scale !=0.0 || p->yscale != 0.0) {
      fprintf (stderr, "\nCan't supply both height and scale\n");
      return 0;
    }
  if (p->scale != 0.0)
    if (p->xscale != 0.0 || p->yscale != 0.0) {
      fprintf (stderr, "\nCan't supply overall scale along with axis scales");
      return 0;
    }
  return 1;
}

static int parse_one_dim_word (char **start, char *end)
{
  int i;
  char *dimension_string;
  char *save = *start;
  skip_white(start, end);

  if ((dimension_string = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "\nExpecting a dimension here\n");
    dump(*start, end);
  }
  for (i=0; i<sizeof(dimensions)/sizeof(dimensions[0]); i++) {
    if (!strcmp (dimensions[i].s, dimension_string))
      break;
  }
  if (i == sizeof(dimensions)/sizeof(dimensions[0])) {
    fprintf (stderr, "\n%s: Invalid dimension\n", dimension_string);
    RELEASE (dimension_string);
    *start = save;
    return -1;
  }
  RELEASE (dimension_string);
  return dimensions[i].key;
}

struct {
  char *s;
  double units;
  int true;
} units[] = {
  {"pt", (72.0/72.27), 0},
  {"in", (72.0), 0},
  {"cm", (72.0/2.54), 0},
  {"mm", (72.0/25.4), 0},
  {"truept", (72.0/72.27), 1},
  {"truein", (72.0), 1},
  {"truecm", (72.0/2.54), 1},
  {"truemm", (72.0/25.4), 1}
};
  
double parse_one_unit (char **start, char *end)
{
  int i;
  char *unit_string, *save = *start;
  skip_white(start, end);
  if ((unit_string = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "\nExpecting a unit here (e.g., in, cm, pt)\n");
    dump(*start, end);
  }
  for (i=0; i<sizeof(units)/sizeof(units[0]); i++) {
    if (!strcmp (units[i].s, unit_string))
      break;
  }
  if (i == sizeof(units)/sizeof(units[0])) {
    fprintf (stderr, "\n%s: Invalid unit of measurement (should be in, cm, pt, etc.)\n", unit_string);
    *start = save; 
    dump (*start, end);
    RELEASE (unit_string);
    return -1.0;
  }
  RELEASE (unit_string);
  if (units[i].true)
    return units[i].units;
  else 
    return units[i].units*dvi_tell_mag();
}

static int parse_dimension (char **start, char *end,
			    struct xform_info *p)
{
  char *number_string, *save = *start;
  double units = 0.0;
  int key;
  save = *start; 
  skip_white(start, end);
  if ((key = parse_one_dim_word(start, end)) < 0 ||
      (number_string = parse_number(start, end)) == NULL) {
    fprintf (stderr, "\nExpecting a dimension or transformation keyword\nfollowed by a number here:\n");
    dump (*start, end);
    *start = save;
    return 0;
  }
  skip_white(start, end);
  if (dimensions[key].hasdimension &&
      (units = parse_one_unit(start, end)) < 0.0) {
    RELEASE (number_string);
    fprintf (stderr, "\nExpecting a unit here\n");
    dump (*start, end);
    *start = save;
    return 0;
  }
  switch (key) {
  case WIDTH:
    if (p->width != 0.0)
      fprintf (stderr, "\nDuplicate width specified\n");
    p->width = atof (number_string)*units;
    break;
  case HEIGHT:
    if (p->height != 0.0)
      fprintf (stderr, "\nDuplicate height specified\n");
    p->height = atof (number_string)*units;
    break;
  case DEPTH:
    if (p->depth != 0.0)
      fprintf (stderr, "\nDuplicate depth specified\n");
    p->depth = atof (number_string)*units;
    break;
  case SCALE:
    if (p->scale != 0.0)
      fprintf (stderr, "\nDuplicate depth specified\n");
    p->scale = atof (number_string);
    break;
  case XSCALE:
    if (p->xscale != 0.0)
      fprintf (stderr, "\nDuplicate xscale specified\n");
    p->xscale = atof (number_string);
    break;
  case YSCALE:
    if (p->yscale != 0.0)
      fprintf (stderr, "\nDuplicate yscale specified\n");
    p->yscale = atof (number_string);
    break;
  case ROTATE:
    if (p->rotate != 0)
      fprintf (stderr, "\nDuplicate rotation specified\n");
    p->rotate = atof (number_string) * M_PI / 180.0;
    break;
  }
  RELEASE(number_string);
  skip_white(start, end);
  return 1;
}


static void do_pagesize(char **start, char *end)
{
  struct xform_info *p;
  p = new_xform_info();
  skip_white(start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find a valid set of dimensions here\n");
      release_xform_info (p);
      dump (*start, end);
      return;
    }
  }
  if (p->scale != 0.0 || p->xscale != 0.0 || p->yscale != 0.0) {
    fprintf (stderr, "\nScale meaningless for pagesize\n");
    release_xform_info (p);
    return;
  }
  if (p->width == 0.0 || p->depth + p->height == 0.0) {
    fprintf (stderr, "\nPage cannot have a zero dimension\n");
    release_xform_info (p);
    return;
  }
  dev_set_page_size (p->width, p->depth + p->height);
  release_xform_info (p);
  parse_crap(start, end);
  return;
}


static void do_ann(char **start, char *end)
{
  pdf_obj *result, *rectangle;
  char *name;
  struct xform_info *p;
#ifdef MEM_DEBUG
MEM_START
#endif
  p = new_xform_info();
  skip_white(start, end);
  name = parse_opt_ident(start, end);
  skip_white(start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find a valid dimension here\n");
      release_xform_info (p);
      dump (*start, end);
      return;
    }
  }
  if (p->scale != 0.0 || p->xscale != 0.0 || p->yscale != 0.0) {
    fprintf (stderr, "\nScale meaningless for annotations\n");
    release_xform_info (p);
    return;
  }
  if (p->width == 0.0 || p->depth + p->height == 0.0) {
    fprintf (stderr, "Special ann: Rectangle has a zero dimension\n");
    release_xform_info (p);
    return;
  }
  if ((result = parse_pdf_dict(start, end)) == NULL) {
    fprintf (stderr, "Ignoring invalid dictionary\n");
    release_xform_info (p);
    return;
  };
  rectangle = pdf_new_array();
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x(),0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()-p->depth,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x()+p->width,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()+p->height,0.1)));
  release_xform_info (p);
  pdf_add_dict (result, pdf_new_name ("Rect"),
		rectangle);
  pdf_doc_add_to_page_annots (pdf_ref_obj (result));

  if (name != NULL) {
    add_reference (name, result, NULL);
    /* An annotation is treated differently from a cos object.
       The previous line adds both a direct link and an indirect link
       to "result".  For cos objects this prevents the direct link
       prevents the object from being flushed immediately.  
       So that an "ann" doesn't behave like an OBJ, the ANN should be
       immediately unlinked.  Otherwise the ANN would have to be
       closed later. This seems awkward, but an annotation is always
       considered to be complete */
    release_reference (name);
    RELEASE (name);
  }
  else 
    pdf_release_obj (result);
  parse_crap(start, end);
#ifdef MEM_DEBUG
MEM_END
#endif
  return;
}

static void do_bgcolor(char **start, char *end)
{
  char *save = *start;
  pdf_obj *color;
  skip_white(start, end);
  if ((color = parse_pdf_object(start, end)) == NULL ||
      (color -> type != PDF_ARRAY && 
       color -> type != PDF_NUMBER )) {
    fprintf (stderr, "\nSpecial: background color: Expecting color specified by an array or number\n");
    *start = save;
    dump (*start, end);
    return;
  }
  if (color -> type == PDF_ARRAY) {
    int i;
    for (i=1; i<=4; i++) {
      if (pdf_get_array (color, i) == NULL ||
	  pdf_get_array (color, i) -> type != PDF_NUMBER)
	break;
    }
    if (i < 4 || i > 5) {
      fprintf (stderr, "\nSpecial: begincolor: Expecting either RGB or CMYK color array\n");
      *start = save;
      dump (*start, end);
      return;
    }
    if (i == 4) {
      dev_bg_rgb_color (pdf_number_value (pdf_get_array (color,1)),
			pdf_number_value (pdf_get_array (color,2)),
			pdf_number_value (pdf_get_array (color,3)));
    }
    if (i == 5) {
      dev_bg_cmyk_color (pdf_number_value (pdf_get_array (color,1)),
			 pdf_number_value (pdf_get_array (color,2)),
			 pdf_number_value (pdf_get_array (color,3)),
			 pdf_number_value (pdf_get_array (color,4)));
    }
  } else {
    dev_bg_gray (pdf_number_value (color));
  }
  pdf_release_obj (color);
  return;
}

static void do_bcolor(char **start, char *end)
{
  char *save = *start;
  pdf_obj *color_array;
#ifdef MEM_DEBUG
  MEM_START
#endif MEM_DEBUG
  skip_white(start, end);
  if ((color_array = parse_pdf_object(start, end)) == NULL ||
      (color_array -> type != PDF_ARRAY && 
       color_array -> type != PDF_NUMBER )) {
    fprintf (stderr, "\nSpecial: begincolor: Expecting color specified by an array or number\n");
    *start = save;
    dump (*start, end);
    return;
  }
  if (color_array -> type == PDF_ARRAY) {
    int i;
    for (i=1; i<=4; i++) {
      if (pdf_get_array (color_array, i) == NULL ||
	  pdf_get_array (color_array, i) -> type != PDF_NUMBER)
	break;
    }
    if (i < 4 || i > 5) {
      fprintf (stderr, "\nSpecial: begincolor: Expecting either RGB or CMYK color array\n");
      dump (*start, end);
      *start = save;
      return;
    }
    if (i == 4) {
      dev_begin_rgb_color (pdf_number_value (pdf_get_array (color_array,1)),
			   pdf_number_value (pdf_get_array (color_array,2)),
			   pdf_number_value (pdf_get_array (color_array,3)));
    }
    if (i == 5) {
      dev_begin_cmyk_color (pdf_number_value (pdf_get_array (color_array,1)),
			    pdf_number_value (pdf_get_array (color_array,2)),
			    pdf_number_value (pdf_get_array (color_array,3)),
			    pdf_number_value (pdf_get_array (color_array,4)));
    }
  } else {
    dev_begin_gray (pdf_number_value (color_array));
  }
  pdf_release_obj (color_array);
#ifdef MEM_DEBUG
  MEM_END
#endif MEM_DEBUG
  return;
}

static void do_bgray(char **start, char *end)
{
  char *number_string;
  skip_white(start, end);
  if ((number_string = parse_number (start, end)) == NULL) {
    fprintf (stderr, "\nSpecial: begingray: Expecting a numerical grayscale specification\n");
    return;
  }
  dev_begin_gray (atof (number_string));
  RELEASE (number_string);
  return;
}

static void do_ecolor(void)
{
#ifdef MEM_DEBUG
  fprintf (debugfile, "(do_ecolor)\n");
#endif MEM_DEBUG
  dev_end_color();
}

static void do_egray(void)
{
  dev_end_color();
}

static void do_bxform (char **start, char *end)
{
  char *save = *start;
  struct xform_info *p;
  p = new_xform_info ();
#ifdef MEM_DEBUG
MEM_START
#endif
  skip_white (start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white (start, end);
    if (!parse_dimension (start, end, p)) {
      fprintf (stderr, "\nFailed to find transformation parameters\n");
      *start = save;
      dump (*start, end);
      release_xform_info (p);
      return;
    }
  }
  if (!validate_image_xform_info (p)) {
    fprintf (stderr, "\nSpecified dimensions are inconsistent\n");
    fprintf (stderr, "\nSpecial will be ignored\n");
    *start = save;
    release_xform_info (p);
    dump (*start, end);
    return;
  }
  if (p -> width != 0.0 || p -> height != 0.0 || p -> depth != 0.0) {
    fprintf (stderr, "Special: bt: width, height, and depth are meaningless\n");
    *start = save;
    release_xform_info (p);
    dump (*start, end);
    return;
  }
  if (p -> scale != 0.0) {
    p->xscale = p->scale;
    p->yscale = p->scale;
  }
  if (p -> xscale == 0.0)
    p->xscale = 1.0;
  if (p -> yscale == 0.0)
    p->yscale = 1.0;
  dev_begin_xform (p->xscale, p->yscale, p->rotate);
  release_xform_info (p);
#ifdef MEM_DEBUG
MEM_END
#endif
  return;
}

static void do_exform(void)
{
#ifdef MEM_DEBUG
MEM_START
#endif
  dev_end_xform();
#ifdef MEM_DEBUG
MEM_END
#endif
}

static void do_outline(char **start, char *end)
{
  pdf_obj *level, *result;
  char *save; 
  static int lowest_level = 255;
#ifdef MEM_DEBUG
    fprintf (debugfile, "(do_outline)\n");
#endif
  skip_white(start, end);
  save = *start; 
  if ((level = parse_pdf_object(start, end)) == NULL) {
    fprintf (stderr, "\nExpecting number for object level.\n");
    dump (*start, end);
    return;
  }
  if ((level -> type) != PDF_NUMBER) {
    fprintf (stderr, "\nExpecting number for object level.\n");
    pdf_release_obj (level);
    *start = save;
    return;
  }
   /* Make sure we know where the starting level is */
  if ( (int) pdf_number_value (level) < lowest_level)
     lowest_level = (int) pdf_number_value (level);
   
  if ((result = parse_pdf_dict(start, end)) == NULL) {
    fprintf (stderr, "Ignoring invalid dictionary\n");
    return;
  };
  parse_crap(start, end);
  pdf_doc_change_outline_depth ((int)pdf_number_value(level)-lowest_level+1);
  pdf_release_obj (level);
  pdf_doc_add_outline (result);
  return;
}

static void do_article(char **start, char *end)
{
  char *name, *save = *start;
  pdf_obj *info_dict;
  skip_white (start, end);
  if (*((*start)++) != '@' || (name = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "Article name expected.\n");
    *start = save;
    dump(*start, end);
    return;
  }
  if ((info_dict = parse_pdf_dict(start, end)) == NULL) {
    RELEASE (name);
    fprintf (stderr, "Ignoring invalid dictionary\n");  
    return;
  }
  pdf_doc_start_article (name, pdf_link_obj(info_dict));
  add_reference (name, info_dict, NULL);
  RELEASE (name);
  parse_crap(start, end);
  return;
}

static void do_bead(char **start, char *end)
{
  pdf_obj *bead_dict, *rectangle, *article, *info_dict;
  char *name, *save = *start;
  struct xform_info *p;
  skip_white(start, end);
  if (*((*start)++) != '@' || (name = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "Article reference expected.\nWhich article does this go with?\n");
    *start = save;
    dump(*start, end);
    return;
  }
  skip_white(start, end);
  p = new_xform_info();
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find a dimension for this bead\n");
      release_xform_info (p);
      return;
    }
  }
  if (p->scale != 0.0 || p->xscale != 0.0 || p->yscale != 0.0) {
    fprintf (stderr, "\nScale meaningless for annotations\n");
    release_xform_info (p);
    return;
  }
  if (p->width == 0.0 || p->depth + p->height == 0.0) {
    fprintf (stderr, "Special bead: Rectangle has a zero dimension\n");
    release_xform_info (p);
    return;
  }
  skip_white (start, end);
  if (**start == '<')
    info_dict = parse_pdf_dict (start, end);
  else
    info_dict = pdf_new_dict();
  /* Does this article exist yet */
  if ((article = lookup_object (name)) == NULL) {
    pdf_doc_start_article (name, pdf_link_obj (info_dict));
    add_reference (name, info_dict, NULL);
  } else {
    pdf_merge_dict (article, info_dict);
    pdf_release_obj (info_dict);
  }
  bead_dict = pdf_new_dict ();
  rectangle = pdf_new_array();
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x(),0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()-p->depth,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x()+p->width,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()+p->height,0.1)));
  release_xform_info(p);
  pdf_add_dict (bead_dict, pdf_new_name ("R"),
		rectangle);
  pdf_add_dict (bead_dict, pdf_new_name ("P"),
		pdf_doc_this_page_ref());
  pdf_doc_add_bead (name, bead_dict);
  if (name != NULL) {
    RELEASE (name);
  }
  return;
}

static void do_epdf (char **start, char *end, double x_user, double y_user)
{
  char *filename, *objname;
  pdf_obj *filestring;
  pdf_obj *trailer, *result;
  struct xform_info *p;
#ifdef MEM_DEBUG
MEM_START
#endif
  skip_white(start, end);
  objname = parse_opt_ident(start, end);
  p = new_xform_info ();
  skip_white(start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find dimensions for encapsulated figure\n");
      release_xform_info (p);
      return;
    }
  }
  if (!validate_image_xform_info (p)) {
    fprintf (stderr, "\nSpecified dimensions are inconsistent\n");
    fprintf (stderr, "\nSpecial will be ignored\n");
    release_xform_info (p);
    return;
  }
  if (*start < end && (filestring = parse_pdf_string(start, end)) !=
      NULL) {
    filename = pdf_string_value(filestring);
    fprintf (stderr, "(%s)", filename);
    if (debug) fprintf (stderr, "Opening %s\n", filename);
#ifdef MEM_DEBUG
    fprintf (debugfile, "Opending file and Reading trailer\n");
#endif
    if ((trailer = pdf_open (filename)) == NULL) {
      fprintf (stderr, "\nSpecial ignored\n");
      release_xform_info (p);
      return;;
    };
    pdf_release_obj (filestring);
    result = pdf_include_page(trailer, x_user, y_user, p);
    release_xform_info(p);
    pdf_release_obj (trailer);
    pdf_close ();
  } else
    {
      fprintf (stderr, "No file name found in special\n");
      release_xform_info (p);
      dump(*start, end);
      return;
    }
  if (result == NULL) {
    fprintf (stderr, "\nEPDF special ignored\n");
    return;
  }
  if (objname != NULL) {
    add_reference (objname, result,
		   pdf_name_value(pdf_lookup_dict(pdf_stream_dict(result), "Name")));
    /* An annotation is treated differently from a cos object.
       The previous line adds both a direct link and an indirect link
       to "result".  For cos objects this prevents the direct link
       prevents the object from being flushed immediately.  
       So that an "ann" doesn't behave like an OBJ, the ANN should be
       immediately unlinked.  Otherwise the ANN would have to be
       closed later. This seems awkward, but an annotation is always
       considered to be complete */
    release_reference (objname);
    RELEASE (objname);
  }
  else
    pdf_release_obj (result);
#ifdef MEM_DEBUG
MEM_END
#endif
}

static void do_image (char **start, char *end, double x_user, double y_user)
{
  char *filename, *objname;
  pdf_obj *filestring, *result;
  struct jpeg *jpeg;
  struct xform_info *p;
  p = new_xform_info();
#ifdef MEM_DEBUG
MEM_START
#endif
  skip_white(start, end);
  objname = parse_opt_ident(start, end);
  skip_white(start, end);

  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find dimensions for encapsulated image\n");
      release_xform_info (p);
      return;
    }
  }
  if (!validate_image_xform_info (p)) {
    fprintf (stderr, "\nSpecified dimensions are inconsistent\n");
    fprintf (stderr, "\nSpecial will be ignored\n");
    release_xform_info (p);
    return;
  }
  if (*start < end && (filestring = parse_pdf_string(start, end)) !=
      NULL) {
    filename = pdf_string_value(filestring);
    fprintf (stderr, "(%s)", filename);
    if (debug) fprintf (stderr, "Opening %s\n", filename);
    if ((jpeg = jpeg_open(filename)) == NULL) {
      fprintf (stderr, "\nSpecial ignored\n");
      release_xform_info (p);
      return;
    };
    pdf_release_obj (filestring);
    result = jpeg_build_object(jpeg, x_user, y_user, p);
    release_xform_info (p);
    jpeg_close (jpeg);
  } else
    {
      fprintf (stderr, "No file name found in special\n");
      release_xform_info (p);
      dump(*start, end);
      return;
    }
  if (result == NULL) {
    fprintf (stderr, "\nSpecial ignored\n");
    return;
  }
  if (objname != NULL) {
    add_reference (objname, result, 
		   pdf_name_value(pdf_lookup_dict(pdf_stream_dict(result), "Name")));
    /* Read the explanation for the next line in do_annot() */
    release_reference (objname);
    RELEASE (objname);
  }
  else
    pdf_release_obj (result);
#ifdef MEM_DEBUG
MEM_END
#endif
}

static void do_dest(char **start, char *end)
{
  pdf_obj *name;
  pdf_obj *array;
  skip_white(start, end);
  if ((name = parse_pdf_string(start, end)) == NULL) {
    fprintf (stderr, "\nPDF string expected and not found.\n");
    fprintf (stderr, "Special dest: ignored\n");
    dump(*start, end);
    return;
  }
  if ((array = parse_pdf_array(start, end)) == NULL)
    return;

  pdf_doc_add_dest (pdf_obj_string_value(name), pdf_obj_string_length(name), array);
  pdf_release_obj (name);
  pdf_release_obj (array);
}

static void do_docinfo(char **start, char *end)
{
  pdf_obj *result;
  if ((result = parse_pdf_dict(start, end)) != NULL) {
    pdf_doc_merge_with_docinfo (result);
    parse_crap(start, end);
  } else {
    fprintf (stderr, "\nSpecial: docinfo: Dictionary expected and not found\n");
    dump (*start, end);
  }
  pdf_release_obj (result);
  return;
}

static void do_docview(char **start, char *end)
{
  pdf_obj *result;
  if ((result = parse_pdf_dict(start, end)) != NULL) {
    pdf_doc_merge_with_catalog (result);
    parse_crap(start, end);
  } else {
    fprintf (stderr, "\nSpecial: docview: Dictionary expected and not found\n");
    dump (*start, end);
  }
  pdf_release_obj (result);
  return;
}


static void do_close(char **start, char *end)
{
  char *name;
  skip_white(start, end);
  if ((name = parse_pdf_reference(start, end)) != NULL) {
    release_reference (name);
    RELEASE (name);
  }
  parse_crap(start, end);
  return;
}

static void do_obj(char **start, char *end)
{
  pdf_obj *result;
  char *name;
  skip_white(start, end);
  name = parse_opt_ident(start, end);
  if ((result = parse_pdf_object(start, end)) == NULL) {
    fprintf (stderr, "Special object: Ignored.\n");
    return;
  };

  parse_crap(start, end);
  
  if (name != NULL) {
    add_reference (name, result, NULL);
    RELEASE (name);
  }
  return;
}

static void do_content(char **start, char *end)
{
  skip_white(start, end);
  pdf_doc_add_to_page (*start, end-*start);
}


static int is_pdf_special (char **start, char *end)
{
  skip_white(start, end);
  if (end-*start >= strlen ("pdf:") &&
      !strncmp (*start, "pdf:", strlen("pdf:"))) {
    *start += strlen("pdf:");
    return 1;
  }
  return 0;
}
/* Unfortunately, already defined in WIN32 */
#ifdef OUT
#undef OUT
#endif

#define ANN 1
#define OUT 2
#define ARTICLE 3
#define DEST 4
#define DOCINFO 7
#define DOCVIEW 8
#define OBJ 9
#define CONTENT 10
#define PUT 11
#define CLOSE 12
#define BOP 13
#define EOP 14
#define BEAD 15
#define EPDF 16
#define IMAGE 17
#define BCOLOR 18
#define ECOLOR 19
#define BGRAY  20
#define EGRAY  21
#define BGCOLOR 22
#define BXFORM 23
#define EXFORM 24
#define PAGESIZE 25
#define BXOBJ 26
#define EXOBJ 27
#define UXOBJ 28

struct pdfmark
{
  char *string;
  int value;
} pdfmarks[] = {
  {"ann", ANN},
  {"annot", ANN},
  {"annotate", ANN},
  {"annotation", ANN},
  {"out", OUT},
  {"outline", OUT},
  {"art", ARTICLE},
  {"article", ARTICLE},
  {"bead", BEAD},
  {"thread", BEAD},
  {"dest", DEST},
  {"docinfo", DOCINFO},
  {"docview", DOCVIEW},
  {"obj", OBJ},
  {"object", OBJ},
  {"content", CONTENT},
  {"put", PUT},
  {"close", CLOSE},
  {"bop", BOP},
  {"eop", EOP},
  {"epdf", EPDF},
  {"image", IMAGE},
  {"img", IMAGE},
  {"bc", BCOLOR},
  {"bcolor", BCOLOR},
  {"begincolor", BCOLOR},
  {"ec", ECOLOR},
  {"ecolor", ECOLOR},
  {"endcolor", ECOLOR},
  {"bg", BGRAY},
  {"bgray", BGRAY},
  {"begingray", BGRAY},
  {"eg", EGRAY},
  {"egray", EGRAY},
  {"endgray", EGRAY},
  {"bgcolor", BGCOLOR},
  {"bgc", BGCOLOR},
  {"bbc", BGCOLOR},
  {"bbg", BGCOLOR},
  {"pagesize", PAGESIZE},
  {"begintransform", BXFORM},
  {"begintrans", BXFORM},
  {"btrans", BXFORM},
  {"bt", BXFORM},
  {"endtransform", EXFORM},
  {"endtrans", EXFORM},
  {"etrans", EXFORM},
  {"et", EXFORM},
  {"beginxobj", BXOBJ},
  {"bxobj", BXOBJ},
  {"endxobj", EXOBJ},
  {"exobj", EXOBJ},
  {"usexobj", UXOBJ},
  {"uxobj", UXOBJ}
};

static int parse_pdfmark (char **start, char *end)
{
  char *save;
  int i;
  if (verbose) {
    fprintf (stderr, "\nparse_pdfmark:");
    dump (*start, end);
  }
  skip_white(start, end);
  if (*start >= end) {
    fprintf (stderr, "Special ignored...no pdfmark found\n");
    return -1;
  }
  
  save = *start;
  while (*start < end && isalpha (**start))
    (*start)++;
  for (i=0; i<sizeof(pdfmarks)/sizeof(struct pdfmark); i++) {
    if (*start-save == strlen (pdfmarks[i].string) &&
	!strncmp (save, pdfmarks[i].string,
		  strlen(pdfmarks[i].string)))
      return pdfmarks[i].value;
  }
  *start = save;
  fprintf (stderr, "\nExpecting pdfmark (and didn't find one)\n");
  dump(*start, end);
  return -1;
}

struct named_reference 
{
  char *name;
  char *res_name;
  pdf_obj *object_ref;
  pdf_obj *object;
} *named_references = NULL;
static unsigned long number_named_references = 0, max_named_objects = 0;

static void add_reference (char *name, pdf_obj *object, char *res_name)
{
  int i;
  if (number_named_references >= max_named_objects) {
    max_named_objects += NAMED_OBJ_ALLOC_SIZE;
    named_references = RENEW (named_references, max_named_objects,
			      struct named_reference);
  }
  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i != number_named_references) {
    fprintf (stderr, "\nWarning: @%s: Duplicate named reference ignored\n", name);
  }
  named_references[number_named_references].name = NEW (strlen
							(name)+1,
							char);
  strcpy (named_references[number_named_references].name, name);
  if (res_name != NULL && strlen(res_name) != 0) {
    named_references[number_named_references].res_name=NEW(strlen(name)+1, char);
    strcpy (named_references[number_named_references].res_name,
	    res_name);
  } else {
    named_references[number_named_references].res_name = NULL;
  }
  named_references[number_named_references].object_ref = pdf_ref_obj(object);
  named_references[number_named_references].object = object;
  number_named_references+=1;
}
/* The following routine returns copies, not the original object */
static pdf_obj *lookup_reference(char *name)
{
  int i;
  /* First check for builtins first */
  if (!strcmp (name, "ypos")) {
    return pdf_new_number(ROUND(dev_tell_y(),0.1));
  }
  if (!strcmp (name, "xpos")) {
    return pdf_new_number(ROUND(dev_tell_x(),0.1));
  }
  if (!strcmp (name, "thispage")) {
    return pdf_doc_this_page_ref();
  }
  if (!strcmp (name, "prevpage")) {
    return pdf_doc_prev_page_ref();
  }
  if (!strcmp (name, "nextpage")) {
    return pdf_doc_prev_page_ref();
  }
  if (!strcmp (name, "pages")) {
    return pdf_ref_obj (pdf_doc_page_tree());
  }
  if (!strcmp (name, "names")) {
    return pdf_ref_obj (pdf_doc_names());
  }
  if (!strcmp (name, "resources")) {
    return pdf_ref_obj (pdf_doc_current_page_resources());
  }
  if (!strcmp (name, "catalog")) {
    return pdf_ref_obj (pdf_doc_catalog());
  }

  if (strlen (name) > 4 &&
      !strncmp (name, "page", 4) &&
      is_a_number (name+4)) {
    return pdf_doc_ref_page(atoi (name+4));
  }
  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i == number_named_references)
    return NULL;
  return (pdf_link_obj (named_references[i].object_ref));
}

static pdf_obj *lookup_object(char *name)
{
  int i;
  if (!strcmp (name, "thispage")) {
    return pdf_doc_this_page();
  }
  if (!strcmp (name, "pages")) {
    return pdf_doc_page_tree();
  }
  if (!strcmp (name, "names")) {
    return pdf_doc_names();
  }
  if (!strcmp (name, "resources")) {
    return pdf_doc_current_page_resources();
  }
  if (!strcmp (name, "catalog")) {
    return pdf_doc_catalog();
  }

  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i == number_named_references)
    return NULL;
  if (named_references[i].object == NULL)
    fprintf (stderr, "lookup_object: Referenced object not defined or already closed\n");
  return (named_references[i].object);
}

char *lookup_ref_res_name(char *name)
{
  int i;
  for (i=0; i<number_named_references; i++) {
  if (named_references[i].name != NULL)
    if (named_references[i].name != NULL &&
	!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i == number_named_references)
    return NULL;
  if (named_references[i].res_name == NULL)
    fprintf (stderr, "lookup_object: Referenced object not useable as a form!\n");
  return (named_references[i].res_name);
}

static void release_reference (char *name)
{
  int i;
  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i == number_named_references) {
    fprintf (stderr, "\nrelease_reference: tried to release nonexistent reference\n");
    return;
  }
  if (named_references[i].object != NULL) {
    pdf_release_obj (named_references[i].object);
    named_references[i].object = NULL;
  }
  else
    fprintf (stderr, "\nrelease_reference: @%s: trying to close an object twice?\n", name);
}



void pdf_finish_specials (void)
{
  int i;
  /* Flush out any pending objects that weren't properly closeed.
     Threads never get closed.  Is this a bug? */
  for (i=0; i<number_named_references; i++) {
    pdf_release_obj (named_references[i].object_ref);
    if (named_references[i].object != NULL) {
      pdf_release_obj (named_references[i].object);
      named_references[i].object = NULL;
    }
    RELEASE (named_references[i].name);
  }
  if (number_named_references > 0)
    RELEASE (named_references);
}

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user)
{
  int pdfmark;
  char *start = buffer, *end;
  end = buffer + size;

#ifdef MEM_DEBUG
MEM_START
#endif

  if (!is_pdf_special(&start, end)) {
    fprintf (stderr, "\nNon PDF special ignored\n");
    dump (start, end);
    return;
  }
  /* Must have a pdf special */
  if ((pdfmark = parse_pdfmark(&start, end)) < 0)
    {
      fprintf (stderr, "\nSpecial ignored.\n");
      return;
    }
  if (verbose)
    fprintf (stderr, "pdfmark = %d\n", pdfmark);
  switch (pdfmark) {
  case ANN:
    do_ann(&start, end);
    break;
  case OUT:
    do_outline(&start, end);
    break;
  case ARTICLE:
    do_article(&start, end);
    break;
  case BEAD:
    do_bead(&start, end);
    break;
  case DEST:
    do_dest(&start, end);
    break;
  case DOCINFO:
    do_docinfo(&start, end);
    break;
  case DOCVIEW:
    do_docview(&start, end);
    break;
  case OBJ:
    do_obj(&start, end);
    break;
  case CONTENT:
    do_content(&start, end);
    break;
  case PUT:
    do_put(&start, end);
    break;
  case CLOSE:
    do_close(&start, end);
    break;
  case BOP:
    do_bop(&start, end);
    break;
  case EOP:
    do_eop(&start, end);
    break;
  case EPDF:
    do_epdf(&start, end, x_user, y_user);
    break;
  case IMAGE:
    do_image(&start, end, x_user, y_user);
    break;
  case BGCOLOR:
    do_bgcolor (&start, end);
    break;
  case BCOLOR:
    do_bcolor (&start, end);
    break;
  case ECOLOR:
    do_ecolor ();
    break;
  case BGRAY:
    do_bgray (&start, end);
    break;
  case EGRAY:
    do_egray ();
    break;
  case BXFORM:
    do_bxform (&start, end);
    break;
  case EXFORM:
    do_exform ();
    break;
  case PAGESIZE:
    do_pagesize(&start, end);
    break;
  case BXOBJ:
    do_bxobj (&start, end, x_user, y_user);
    break;
  case EXOBJ:
    do_exobj ();
    break;
  case UXOBJ:
    do_uxobj (&start, end, x_user, y_user);
    break;
  }
#ifdef MEM_DEBUG
MEM_END
#endif
}

/* Compute a transformation matrix
   transformations are applied in the following
   order: scaling, rotate, displacement. */
void add_xform_matrix (double xoff, double yoff,
		       double xscale, double yscale,
		       double rotate) 
{
  double c, s;
  c = ROUND(cos(rotate),1e-5);
  s = ROUND(sin(rotate),1e-5);
  sprintf (work_buffer, " %g %g %g %g %g %g cm", 
	   c*xscale, s*xscale, -s*yscale, c*yscale, xoff, yoff);
  pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
}


static num_images = 0;
pdf_obj *jpeg_build_object(struct jpeg *jpeg, double x_user, double
			   y_user, struct xform_info *p)
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
    if (p->scale != 0) {
      xscale *= p->scale;
      yscale *= p->scale;;
    }
    if (p->xscale != 0) {
      xscale *= p->xscale;
    }
    if (p->yscale != 0) {
      yscale *= p->yscale;
    }
    if (p->width != 0.0) {
      xscale = p->width;
      if (p->height == 0.0)
	yscale = xscale;
    }
    if (p->height != 0.0) {
      yscale = p->height;
      if (p->width == 0.0)
	xscale = p->yscale;
    }
  }
  pdf_doc_add_to_page (" q", 2);
  add_xform_matrix (x_user, y_user, xscale, yscale, p->rotate);
  if (p->depth != 0.0)
    add_xform_matrix (0.0, -p->depth, 1.0, 1.0, 0.0);
  sprintf (work_buffer, " /Im%d Do Q", num_images);
  pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
  return (xobject);
}

static void do_bxobj (char **start, char *end, double x_user, double y_user)
{
  char *objname;
  pdf_obj *xobject;
  struct xform_info *p;
  skip_white(start, end);
  if ((objname = parse_opt_ident(start, end)) == NULL) {
    fprintf (stderr, "\nSpecial: beginxobj:  A form XObject must be named\n");
    return;
  }
  p = new_xform_info ();
  skip_white(start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find a valid dimension here\n");
      release_xform_info (p);
      dump (*start, end);
      return;
    }
  }
  if (p->scale != 0.0 || p->xscale != 0.0 || p->yscale != 0.0) {
    fprintf (stderr, "\nScale information is meaningless for form xobjects\n");
    return;
  }
  if (p->width == 0.0 || p->depth+p->height == 0.0) {
    fprintf (stderr, "Special: bxobj: Bounding box has a zero dimension\n");
  }
  xobject = begin_form_xobj (x_user, y_user-p->depth, x_user+p->width, y_user+p->height);
  release_xform_info (p);
  add_reference (objname, xobject,
		 pdf_name_value(pdf_lookup_dict(pdf_stream_dict(xobject), "Name")));
  /* Next line has Same explanation as for do_ann.  Clumsy
     has the desired effect.  This module is done with xobject.
     It's still linked in the doc module, which will release
     it when it's finished. */
  release_reference (objname);
  RELEASE (objname);
}

static void do_exobj (void)
{
  end_form_xobj();
}

static void do_uxobj (char **start, char *end, double x_user, double y_user)
{
  char *objname, *res_name;
  pdf_obj *xobj_res;
  skip_white (start, end);
  if ((objname = parse_opt_ident(start, end)) == NULL) {
    fprintf (stderr, "\nSpecial: usexobj:  A form XObject must be named\n");
    return;
  }
  if ((res_name = lookup_ref_res_name (objname)) == NULL) {
    fprintf (stderr, "\nSpecial: usexobj:  Specified XObject doesn't exist: %s\n", 
	     objname);
    return;
  }
  if ((xobj_res = lookup_reference (objname)) == NULL) {
    fprintf (stderr, "\nSpecial: usexobj:  Couldn't find reference to XObject: %s\n",
	     objname);
  }
  RELEASE (objname);
  sprintf (work_buffer, " q 1 0 0 1 %g %g cm /%s Do Q",
	   ROUND(x_user, 0.1), ROUND(y_user, 0.1), res_name);
  pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
  pdf_doc_add_to_page_xobjects (res_name, xobj_res);
}
