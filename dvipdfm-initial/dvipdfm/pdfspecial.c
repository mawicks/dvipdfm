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
#include <stdlib.h>
#include <ctype.h>
#include "pdflimits.h"
#include "pdfspecial.h"
#include "pdfobj.h"
#include "pdfdoc.h"
#include "pdfdev.h"
#include "pdfparse.h"
#include "numbers.h"
#include "mem.h"
#include "dvi.h"
#include "io.h"
#include "jpeg.h"

#define verbose 0
#define debug 0

static void add_reference (char *name, pdf_obj *object);
static void release_reference (char *name);
static pdf_obj *lookup_reference(char *name);
static pdf_obj *lookup_object(char *name);
static void do_content ();
static void do_epdf();
static void do_image();

static void do_bop(char **start, char *end)
{
  if (*start != end)
    pdf_doc_bop (*start, end - *start);
  return;
}

static void do_eop(char **start, char *end)
{
  if (*start != end)
    pdf_doc_eop (*start, end - *start);
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
    release (name);
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
    release (name);
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
    if ((data = parse_pdf_dict (start, end)) == NULL) {
      return;
    }
    pdf_merge_dict (result, data);
    return;
  }
  if (result -> type == PDF_ARRAY) {
    if ((data = parse_pdf_array (start, end)) == NULL) {
      return;
    }
    pdf_add_array (result, data);
    return;
  }
  else {
    fprintf (stderr, "\nSpecial put:  Invalid object type\n");
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
  {"yscale", YSCALE, 0}
};


static struct dimension_info *new_dimension_info (void)
{
  struct dimension_info *result;
  result = NEW (1, struct dimension_info);
  result -> width = 0.0;
  result -> height = 0.0;
  result -> depth = 0.0;
  result -> scale = 0.0;
  result -> xscale = 0.0;
  result -> yscale = 0.0;
  return result;
}

static void release_dimension_info (struct dimension_info *p)
{
  release (p);
  return;
}

static int validate_image_dimension_info (struct dimension_info *p)
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
    release (dimension_string);
    *start = save;
    return -1;
  }
  release (dimension_string);
  return dimensions[i].key;
}

struct {
  char *s;
  double units;
} units[] = {
  {"pt", (72.0/72.27)},
  {"in", (72.0)},
  {"cm", (72.0/2.54)}
};
  
double parse_one_unit (char **start, char *end)
{
  int i;
  char *unit_string;
  skip_white(start, end);
  if ((unit_string = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "\nExpecting a unit here\n");
    dump(*start, end);
  }
  for (i=0; i<sizeof(dimensions)/sizeof(dimensions[0]); i++) {
    if (!strcmp (units[i].s, unit_string))
      break;
  }
  if (i == sizeof(dimensions)/sizeof(dimensions[0])) {
    fprintf (stderr, "%s: Invalid dimension\n", unit_string);
    release (unit_string);
    return -1.0;
  }
  release (unit_string);
  return units[i].units*dvi_tell_mag();
}

static int parse_dimension (char **start, char *end, struct
				  dimension_info *p)
{
  double dimension;
  char *number_string, *save = *start;
  double units;
  int key;
  skip_white(start, end);
  if ((key = parse_one_dim_word(start, end)) < 0 ||
      (number_string = parse_number(start, end)) == NULL) {
    *start = save;
    fprintf (stderr, "\nExpecting a dimension keyword and a number here\n");
    dump (*start, end);
    return 0;
  }
  if (dimensions[key].hasdimension &&
      (units = parse_one_unit(start, end)) < 0.0) {
    release (number_string);
    fprintf (stderr, "\nExpecting a units here\n");
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
  }
  release(number_string);
  skip_white(start, end);
}

static void do_ann(char **start, char *end)
{
  pdf_obj *result, *rectangle, *tmp1, *tmp2;
  char *name, *number_string;
  int dimension;
  struct dimension_info *p;
  p = new_dimension_info();
  skip_white(start, end);
  name = parse_opt_ident(start, end);
  skip_white(start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find a dimension keyword\n");
      return;
    }
  }
  if (p->scale != 0.0 || p->xscale != 0.0 || p->yscale != 0.0) {;
  fprintf (stderr, "\nScale meaningless for annotations\n");
  return;
  }
  if (p->width == 0.0 || p->depth + p->height == 0.0) {
    fprintf (stderr, "Special ann: Rectangle has a zero dimension\n");
    return;
  }
  if ((result = parse_pdf_dict(start, end)) == NULL) {
    fprintf (stderr, "Ignoring invalid dictionary\n");
    return;
  };
  rectangle = pdf_new_array();
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x(),0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()-p->depth,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x()+p->width,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()+p->height,0.1)));
  release_dimension_info (p);
  pdf_add_dict (result, pdf_new_name ("Rect"),
		rectangle);
  pdf_doc_add_to_page_annots (pdf_ref_obj (result));

  if (name != NULL) {
    add_reference (name, result);
    /* An annotation is treated differently from a cos object.
       The previous line adds both a direct link and an indirect link
       to "result".  For cos objects this prevents the direct link
       prevents the object from being flushed immediately.  
       So that an "ann" doesn't behave like an OBJ, the ANN should be
       immediately unlinked.  Otherwise the ANN would have to be
       closed later. This seems awkward, but an annotation is always
       considered to be complete */
    release_reference (name);
    release (name);
  }
  else 
    pdf_release_obj (result);
  parse_crap(start, end);
  return;
}

static void do_bcolor(char **start, char *end)
{
  char *save = *start;
  pdf_obj *color_array, *tmp;
  double r, g, b;
  skip_white(start, end);
  if ((color_array = parse_pdf_object(start, end)) == NULL) {
    fprintf (stderr, "\nSpecial: begincolor: Expecting color specified by an array\n");
    return;
  }
  if (color_array -> type != PDF_ARRAY) {
    fprintf (stderr, "\nSpecial: begincolor: Expecting color specified by an array\n");
    *start = save;
    return;
  }
  {
    int i;
    for (i=1; i<=4; i++) {
      if (pdf_get_array (color_array, i) == NULL)
	break;
    }
    if (i != 4) {
      fprintf (stderr, "\nSpecial: begincolor: Expecting color array with three elements\n");
      return;
    }
    r = pdf_number_value (pdf_get_array (color_array, 1));
    g = pdf_number_value (pdf_get_array (color_array, 2));
    b = pdf_number_value (pdf_get_array (color_array, 3));
    dev_begin_color (r, g, b);
    return;
  }
}

static void do_ecolor(void)
{
  dev_end_color();
}

static void do_outline(char **start, char *end)
{
  pdf_obj *result;
  char *level;
  skip_white(start, end);

  if ((level = parse_ident(start, end)) == NULL ||
      !is_a_number (level)) {
    fprintf (stderr, "\nExpecting number for object level\n");
    dump (*start, end);
    return;
  }
  if ((result = parse_pdf_dict(start, end)) == NULL) {
    fprintf (stderr, "Ignoring invalid dictionary\n");
    return;
  };
  parse_crap(start, end);

  pdf_doc_change_outline_depth (atoi (level));
  release (level);
  pdf_doc_add_outline (result);
  return;
}

static void do_article(char **start, char *end)
{
  char *name, *save = *start;
  pdf_obj *info_dict, *article_dict;
  skip_white (start, end);
  if (*((*start)++) != '@' || (name = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "Article name expected.\n");
    *start = save;
    dump(*start, end);
  }
  if ((info_dict = parse_pdf_dict(start, end)) == NULL) {
    release (name);
    fprintf (stderr, "Ignoring invalid dictionary\n");  
  }
  add_reference (name, pdf_doc_add_article (name, info_dict));
  release (name);
  parse_crap(start, end);
}

static void do_bead(char **start, char *end)
{
  pdf_obj *bead_dict, *rectangle, *tmp1, *tmp2;
  char *name, *number_string, *save = *start;
  int key;
  struct dimension_info *p;
  skip_white(start, end);
  if (*((*start)++) != '@' || (name = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "Article reference expected.\nWhich article does this go with?\n");
    *start = save;
    dump(*start, end);
  }
  skip_white(start, end);
  p = new_dimension_info();
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find a dimension for this bead\n");
      return;
    }
  }
  if (p->scale != 0.0 || p->xscale != 0.0 || p->yscale != 0.0) {
    fprintf (stderr, "\nScale meaningless for annotations\n");
    return;
  }
  if (p->width == 0.0 || p->depth + p->height == 0.0) {
    fprintf (stderr, "Special bead: Rectangle has a zero dimension\n");
    return;
  }
  bead_dict = pdf_new_dict ();
  rectangle = pdf_new_array();
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x(),0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()-p->depth,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_x()+p->width,0.1)));
  pdf_add_array (rectangle, pdf_new_number(ROUND(dev_tell_y()+p->height,0.1)));
  release_dimension_info(p);
  pdf_add_dict (bead_dict, pdf_new_name ("R"),
		rectangle);
  pdf_add_dict (bead_dict, pdf_new_name ("P"),
		pdf_doc_this_page());
  pdf_doc_add_bead (name, bead_dict);
  if (name != NULL) {
    release (name);
  }
  pdf_release_obj (bead_dict);
  parse_crap(start, end);
  return;
}


static void do_epdf (char **start, char *end, double x_user, double y_user)
{
  char *filename, *objname, *number_string;
  pdf_obj *filestring;
  pdf_obj *trailer, *result;
  struct dimension_info *p;
  skip_white(start, end);
  objname = parse_opt_ident(start, end);
  p = new_dimension_info ();
  skip_white(start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find dimensions for encapsulated figure\n");
      return;
    }
  }
  if (!validate_image_dimension_info (p)) {
    fprintf (stderr, "\nSpecified dimensions are inconsistent\n");
    fprintf (stderr, "\nSpecial will be ignored\n");
    return;
  }
  if (*start < end && (filestring = parse_pdf_string(start, end)) !=
      NULL) {
    filename = pdf_string_value(filestring);
    fprintf (stderr, "(%s)", filename);
    if (debug) fprintf (stderr, "Opening %s\n", filename);
    if ((trailer = pdf_open (filename)) == NULL) {
      fprintf (stderr, "\nSpecial ignored\n");
      return;
    };
    pdf_release_obj (filestring);
    result = pdf_include_page(trailer, x_user, y_user, p);
    release_dimension_info(p);
    pdf_release_obj (trailer);
    pdf_close ();
  } else
    {
      fprintf (stderr, "No file name found in special\n");
      dump(*start, end);
      return;
    }
  if (result == NULL) {
    fprintf (stderr, "\nSpecial ignoed\n");
    return;
  }
  if (objname != NULL) {
    add_reference (objname, result);
    /* An annotation is treated differently from a cos object.
       The previous line adds both a direct link and an indirect link
       to "result".  For cos objects this prevents the direct link
       prevents the object from being flushed immediately.  
       So that an "ann" doesn't behave like an OBJ, the ANN should be
       immediately unlinked.  Otherwise the ANN would have to be
       closed later. This seems awkward, but an annotation is always
       considered to be complete */
    release_reference (objname);
    release (objname);
  }
  else
    pdf_release_obj (result);
}

static void do_image (char **start, char *end, double x_user, double y_user)
{
  char *filename, *number_string, *objname;
  pdf_obj *filestring, *result;
  struct jpeg *jpeg;
  struct dimension_info *p;
  p = new_dimension_info();
  skip_white(start, end);
  objname = parse_opt_ident(start, end);
  skip_white(start, end);

  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if (!parse_dimension(start, end, p)) {
      fprintf (stderr, "\nFailed to find dimensions for encapsulated image\n");
      return;
    }
  }
  if (!validate_image_dimension_info (p)) {
    fprintf (stderr, "\nSpecified dimensions are inconsistent\n");
    fprintf (stderr, "\nSpecial will be ignored\n");
    return;
  }
  if (*start < end && (filestring = parse_pdf_string(start, end)) !=
      NULL) {
    filename = pdf_string_value(filestring);
    fprintf (stderr, "(%s)", filename);
    if (debug) fprintf (stderr, "Opening %s\n", filename);
    if ((jpeg = jpeg_open(filename)) == NULL) {
      fprintf (stderr, "\nSpecial ignored\n");
      return;
    };
    pdf_release_obj (filestring);
    result = jpeg_build_object(jpeg, x_user, y_user, p);
    release_dimension_info (p);
    jpeg_close (jpeg);
  } else
    {
      fprintf (stderr, "No file name found in special\n");
      dump(*start, end);
      return;
    }
  if (result == NULL) {
    fprintf (stderr, "\nSpecial ignored\n");
    return;
  }
  if (objname != NULL) {
    add_reference (objname, result);
    /* An annotation is treated differently from a cos object.
       The previous line adds both a direct link and an indirect link
       to "result".  For cos objects this prevents the direct link
       prevents the object from being flushed immediately.  
       So that an "ann" doesn't behave like an OBJ, the ANN should be
       immediately unlinked.  Otherwise the ANN would have to be
       closed later. This seems awkward, but an annotation is always
       considered to be complete */
    release_reference (objname);
    release (objname);
  }
  else
    pdf_release_obj (result);
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
  pdf_obj *result = parse_pdf_dict(start, end);
  pdf_doc_merge_with_docinfo (result);
  parse_crap(start, end);
  return;
}

static void do_docview(char **start, char *end)
{
  pdf_obj *result = parse_pdf_dict(start, end);
  pdf_doc_merge_with_catalog (result);
  parse_crap(start, end);
  return;
}


static void do_close(char **start, char *end)
{
  char *name;
  skip_white(start, end);
  if ((name = parse_pdf_reference(start, end)) != NULL) {
    release_reference (name);
    release (name);
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
    add_reference (name, result);
    release (name);
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
  {"eegincolor", ECOLOR}
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
  pdf_obj *object_ref;
  pdf_obj *object;
} named_references[MAX_NAMED_REFERENCES];

static unsigned number_named_references = 0;

static void add_reference (char *name, pdf_obj *object)
{
  int i;
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
  named_references[number_named_references].object_ref = pdf_ref_obj(object);
  named_references[number_named_references].object = object;
  number_named_references+=1;
}
/* The following routine returns copies, not the original object */
static pdf_obj *lookup_reference(char *name)
{
  int i;
  /* First check for builtin */
  if (!strcmp (name, "thispage")) {
    return pdf_doc_this_page();
  }
  if (!strcmp (name, "prevpage")) {
    return pdf_doc_prev_page();
  }
  if (!strcmp (name, "nextpage")) {
    return pdf_doc_prev_page();
  }
  if (!strcmp (name, "ypos")) {
    return pdf_new_number(ROUND(dev_tell_y(),0.1));
  }
  if (!strcmp (name, "xpos")) {
    return pdf_new_number(ROUND(dev_tell_x(),0.1));
  }
  if (!strcmp (name, "resources")) {
    return pdf_ref_obj (pdf_doc_current_page_resources());
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
  if (!strcmp (name, "resources")) {
    return pdf_doc_current_page_resources();
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
    }
    release (named_references[i].name);
  }
}

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user, double x_media, double y_media)
{
  int pdfmark;
  char *start = buffer, *end;
  end = buffer + size;

  if (!is_pdf_special(&start, end)) {
    fprintf (stderr, "\nNon PDF special ignored\n");
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
  case BCOLOR:
    do_bcolor (&start, end);
    break;
  case ECOLOR:
    do_ecolor ();
    break;
  }
}

static pdf_obj *build_scale_array (int a, int b, int c, int d, int e, int f)
{
  pdf_obj *result;
  result = pdf_new_array();
  pdf_add_array (result, pdf_new_number (a));
  pdf_add_array (result, pdf_new_number (b));
  pdf_add_array (result, pdf_new_number (c));
  pdf_add_array (result, pdf_new_number (d));
  pdf_add_array (result, pdf_new_number (e));
  pdf_add_array (result, pdf_new_number (f));
  return result;
}


int num_xobjects = 0;

pdf_obj *pdf_include_page(pdf_obj *trailer, double x_user, double y_user,
			  struct dimension_info *p)
{
  pdf_obj *catalog, *page_tree,
    *kids_ref, *kids;
  pdf_obj *media_box, *resources, *contents, *contents_ref;
  pdf_obj *new_resources;
  pdf_obj *xobj_dict;
  pdf_obj *tmp1, *tmp2;
  double xscale, yscale;
  int i;
  char *key;
  /* Now just lookup catalog location */
  /* Deref catalog */
  catalog = pdf_deref_obj (pdf_lookup_dict (trailer, "Root"));
  if (debug) {
    fprintf (stderr, "Catalog:\n");
    pdf_write_obj (stderr, catalog);
  }

  /* Lookup page tree in catalog */
  page_tree = pdf_deref_obj (pdf_lookup_dict (catalog, "Pages"));

  /* Media box and resources can be inherited so start looking for
     them here */
  media_box = pdf_deref_obj (pdf_lookup_dict (page_tree, "MediaBox"));
  tmp1 = pdf_deref_obj (pdf_lookup_dict (page_tree, "Resources"));
  if (tmp1) {
    resources = tmp1;
  }
  else {
    resources = pdf_new_dict();
  }
  while ((kids_ref = pdf_lookup_dict (page_tree, "Kids")) != NULL) {
    pdf_release_obj (page_tree);
    kids = pdf_deref_obj (kids_ref);
    page_tree = pdf_deref_obj (pdf_get_array(kids, 1));
    pdf_release_obj (kids);
    /* Replace MediaBox if it's here */
    tmp1 = pdf_lookup_dict (page_tree, "MediaBox");
    if (tmp1 && media_box)
      pdf_release_obj (media_box);
    if (tmp1) 
      media_box = tmp1;
    /* Add resources if they're here */
    tmp1 = pdf_lookup_dict (page_tree, "Resources");
    if (tmp1) {
      pdf_merge_dict (tmp1, resources);
      pdf_release_obj (resources);
      resources = tmp1;
    }
  }
  /* At this point, page_tree contains the first page.  media_box and
     resources should also be set. */
  /* Take care of scaling */
  { 
    double bbllx, bblly, bburx, bbury;
    bbllx = pdf_number_value (pdf_get_array (media_box, 1));
    bblly = pdf_number_value (pdf_get_array (media_box, 2));
    bburx = pdf_number_value (pdf_get_array (media_box, 3));
    bbury = pdf_number_value (pdf_get_array (media_box, 4));
    xscale = 1.0;
    yscale = 1.0;
    if (p->scale != 0.0) {
      xscale = p->scale;
      yscale = p->scale;
    }
    if (p->xscale != 0.0) {
      xscale = p->xscale;
    }
    if (p->yscale != 0.0) {
      yscale = p->yscale;
    }
    if (p-> width != 0.0 && bbllx != bburx) {
      xscale = p->width / (bburx - bbllx);
      if (p->height == 0.0)
	yscale = xscale;
    }
    if (p->height != 0.0 && bblly != bbury) {
      yscale = p->height / (bbury - bblly);
      if (p->width == 0.0)
	xscale = yscale;
    }
  }
  contents_ref = pdf_lookup_dict (page_tree, "Contents");
  pdf_release_obj (page_tree);
  contents = pdf_deref_obj (contents_ref);
  pdf_release_obj(contents_ref);  /* Remove "old" reference */
  contents_ref = pdf_ref_obj (contents);  /* Give it a "new" reference */
  xobj_dict = pdf_stream_dict (contents);
  num_xobjects += 1;
  sprintf (work_buffer, "Fm%d", num_xobjects);
  pdf_doc_add_to_page_xobjects (work_buffer, contents_ref);
  pdf_add_dict (xobj_dict, pdf_new_name ("Name"),
		pdf_new_name (work_buffer));
  pdf_add_dict (xobj_dict, pdf_new_name ("Type"),
		pdf_new_name ("XObject"));
  pdf_add_dict (xobj_dict, pdf_new_name ("Subtype"),
		pdf_new_name ("Form"));
  pdf_add_dict (xobj_dict, pdf_new_name ("BBox"), media_box);
  pdf_add_dict (xobj_dict, pdf_new_name ("FormType"), 
		pdf_new_number(1.0));
  tmp1 = build_scale_array (1, 0, 0, 1, 0, 0);
  pdf_add_dict (xobj_dict, pdf_new_name ("Matrix"), tmp1);
  new_resources = pdf_new_dict();
  pdf_add_dict (xobj_dict, pdf_new_name ("Resources"),
		pdf_ref_obj (new_resources));
  for (i=1; (key = pdf_get_dict(resources, i)) != NULL; i++) 
    {
      tmp2 = pdf_deref_obj (pdf_lookup_dict (resources, key));
      tmp1 = pdf_ref_obj (tmp2);
      pdf_release_obj (tmp2);
      pdf_add_dict (new_resources, pdf_new_name (key), tmp1);
      release (key);
    }
  pdf_release_obj (new_resources);
  pdf_release_obj (resources);
  sprintf (work_buffer, " q %g 0 0 %g  %g %g cm /Fm%d Do Q ", xscale,
	   yscale, x_user, y_user-p->depth, num_xobjects);
  pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
  return (contents);
  /* pdf_release_obj(contents); */
}