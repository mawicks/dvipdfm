/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdev.c,v 1.102.4.5 2000/08/03 03:03:08 mwicks Exp $

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
	
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "system.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"
#include "numbers.h"
#include "dvi.h"
#include "tfm.h"
#include "pdfdev.h"
#include "pdfdoc.h"
#include "pdfobj.h"
#include "type1.h"
#include "pkfont.h"
#include "pdfspecial.h"
#include "pdfparse.h"
#include "tpic.h"
#include "htex.h"
#include "mpost.h"
#include "psspecial.h"
#include "colorsp.h"
#include "pdflimits.h"
#include "twiddle.h"

/* Internal functions */
static void dev_clear_color_stack (void);
static void dev_clear_xform_stack (void);

double hoffset = 72.0, voffset=72.0;

static double dvi2pts = 0.0;

 /* Acrobat doesn't seem to like coordinate systems
    that involve scalings around 0.01, so we use
    a scaline of 1.0.  In other words, device units = pts */ 
/* Following dimensions in virtual device coordinates,
which are points */

static double page_width=612.0, page_height=792.0;
int page_size_readonly = 0;

void dev_set_page_size (double width, double height)
{
  if (page_size_readonly) {
    fprintf (stderr, "\nSorry.  Too late to change page size\n");
  } else {
    page_width = width;
    page_height = height;
  }
}

double dev_page_width(void)
{
  page_size_readonly = 1;
  return page_width;
}

double dev_page_height(void)
{
  page_size_readonly = 1;
  return page_height;
}

static int debug = 0, verbose = 0;

void dev_set_verbose (void)
{
  verbose = 1;
}
void dev_set_debug (void)
{
  debug = 1;
}

#define GRAPHICS_MODE 1
#define TEXT_MODE 2
#define STRING_MODE 3

int motion_state = GRAPHICS_MODE; /* Start in graphics mode */

#define FORMAT_BUF_SIZE 4096
static char format_buffer[FORMAT_BUF_SIZE];

/* Coordinate system in the pdf file is setup so that 1 unit in the
   PDF content stream's coordinate system represents 65,800 spt (DVI units).
   Relative motions in the PDF file are printed in decimal
   with no more than two digits after the decimal point.  A
   PDF user coordinate of 0.01 represents 658 DVI units.
   Here are some constants to that effect */

#define PDF_U 65800L
#define CENTI_PDF_U 658
#define PDF_U_TO_A_PTS 1.0002773

double pdf_dev_scale (void)
{
  return  PDF_U_TO_A_PTS;
}


 /* Device coordinates are relative to upper left of page.  One of the
    first things appearing in the page stream is a coordinate transformation
    matrix that forces this to be true.  This coordinate
    transformation is the only place where the paper size is required.
    Unfortunately, positive is up, which doesn't agree with TeX's convention.  */

static spt_t text_xorigin = 0, text_yorigin = 0,
  text_offset = 0;
double text_slant = 0.0, text_extend = 1.0;

unsigned  n_dev_fonts = 0;
unsigned  n_phys_fonts = 0;
int current_font = -1;

#define PHYSICAL 1
#define VIRTUAL 2

#define TYPE1 1
#define PK 2

static struct dev_font {
  char short_name[7];	/* Needs to be big enough to hold name "Fxxx"
			   where xxx is number of largest font */
  unsigned char used_on_this_page, format;
  char *tex_name;
  int tfm_font_id;
  double ptsize;
  spt_t sptsize;
  pdf_obj *font_resource;
  char *used_chars;
  double extend, slant;
  int remap;
} *dev_font = NULL;

static unsigned max_device_fonts = 0;

void need_more_dev_fonts (unsigned n)
{
  if (n_dev_fonts + n > max_device_fonts) {
    max_device_fonts += MAX_FONTS;
    dev_font = RENEW (dev_font, max_device_fonts, struct dev_font);
  }
}

/*
 * reset_text_state() outputs a BT
 * and does any necessary coordinate transformations
 * to get ready to ship out text
 */

static void reset_text_state(void)
{
  int len;
  text_xorigin = 0;
  text_yorigin = 0;
  text_offset = 0;
  /* 
   * We need to reset the line matrix to handle slanted fonts 
   */
  len = sprintf (format_buffer, " BT");
  if (current_font >= 0 && /* If not at top of page */
      (dev_font[current_font].slant != 0.0 ||
       dev_font[current_font].extend != 1.0)) {
    len += sprintf (format_buffer+len, " %.7g 0 %.3g 1 ",
		    dev_font[current_font].extend,
		    dev_font[current_font].slant);
    len += centi_u_to_a (format_buffer+len, IDIVRND (text_xorigin, CENTI_PDF_U));
    format_buffer[len++] = ' ';
    len += centi_u_to_a (format_buffer+len, IDIVRND (text_yorigin, CENTI_PDF_U));
    format_buffer[len++] = ' ';
    format_buffer[len++] = 'T';
    format_buffer[len++] = 'm';
  }
  pdf_doc_add_to_page (format_buffer, len);
}

static void text_mode (void)
{
  switch (motion_state) {
  case STRING_MODE:
    pdf_doc_add_to_page (")]TJ", 4);
  case TEXT_MODE:
    break;
  case GRAPHICS_MODE:
    reset_text_state();
    break;
  }
  motion_state = TEXT_MODE;
  text_offset = 0;
  return;
}

void graphics_mode (void)
{
  switch (motion_state) {
  case GRAPHICS_MODE:
    break;
  case STRING_MODE:
    pdf_doc_add_to_page (")]TJ", 4);
  case TEXT_MODE:
    pdf_doc_add_to_page (" ET", 3);
    break;
  }
  motion_state = GRAPHICS_MODE;
  return;
}

static void string_mode (spt_t xpos, spt_t ypos, double slant, double extend)
{
  spt_t delx, dely;
  int len = 0;
  switch (motion_state) {
  case STRING_MODE:
    break;
  case GRAPHICS_MODE:
    reset_text_state();
    /* Fall through now... */
    /* Following may be necessary after a rule (and also after
       specials) */
  case TEXT_MODE:
    delx = xpos - text_xorigin;
    {
      spt_t rounded_delx, desired_delx;
      spt_t rounded_dely, desired_dely;
      spt_t dvi_xerror, dvi_yerror;

      /* First round dely (it is needed for delx) */
      dely = ypos - text_yorigin;
      desired_dely = dely;
      rounded_dely = IDIVRND(desired_dely, CENTI_PDF_U) * CENTI_PDF_U;
      /* Next round delx, precompensating for line transformation matrix */
      desired_delx = (delx-desired_dely*slant)/extend;
      rounded_delx = IDIVRND(desired_delx, CENTI_PDF_U) * CENTI_PDF_U;
      /* Estimate errors in DVI units */
      dvi_yerror = (desired_dely - rounded_dely);
      dvi_xerror = (extend*(desired_delx - rounded_delx)+slant*dvi_yerror);
      format_buffer[len++] = ' ';
      len += centi_u_to_a (format_buffer+len, rounded_delx/CENTI_PDF_U);
      format_buffer[len++] = ' ';
      len += centi_u_to_a (format_buffer+len, rounded_dely/CENTI_PDF_U);
      pdf_doc_add_to_page (format_buffer, len);
      len = 0;
      pdf_doc_add_to_page (" TD[(", 5);
      text_xorigin = xpos-dvi_xerror;
      text_yorigin = ypos-dvi_yerror;
    }
    text_offset = 0;
    break;
  }
  motion_state = STRING_MODE;
  return;
}

/* The purpose of the following routine is to force a Tf only
   when it's actually necessary.  This became a problem when the
   VF code was added.  The VF spec says to instantiate the
   first font contained in the VF file before drawing a virtual
   character.  However, that font may not be used for
   many characters (e.g. small caps fonts).  For this reason, 
   dev_select_font() should not force a "physical" font selection.
   This routine prevents a PDF Tf font selection until there's
   really a character in that font.  */

static void dev_set_font (int font_id)
{
  int len = 0;
  text_mode();
  len = sprintf (format_buffer, "/%s ", dev_font[font_id].short_name);
  len += centi_u_to_a (format_buffer+len, IDIVRND(dev_font[font_id].sptsize, CENTI_PDF_U));
  format_buffer[len++] = ' ';
  format_buffer[len++] = 'T';
  format_buffer[len++] = 'f';
  if (dev_font[font_id].slant != text_slant ||
      dev_font[font_id].extend != text_extend) {
    len += sprintf (format_buffer+len, " %.7g 0 %.3g 1 ",
		    dev_font[font_id].extend,
		    dev_font[font_id].slant);
    len += centi_u_to_a (format_buffer+len, IDIVRND(text_xorigin, CENTI_PDF_U));
    format_buffer[len++] = ' ';
    len += centi_u_to_a (format_buffer+len, IDIVRND(text_yorigin, CENTI_PDF_U));
    format_buffer[len++] = ' ';
    format_buffer[len++] = 'T';
    format_buffer[len++] = 'm';
     /* There's no longer any uncertainty about where we are */
    text_slant = dev_font[font_id].slant;
    text_extend = dev_font[font_id].extend;
  }
  pdf_doc_add_to_page (format_buffer, len);
  /* Add to Font list in Resource dictionary for this page */
  if (!dev_font[font_id].used_on_this_page) { 
    pdf_doc_add_to_page_fonts (dev_font[font_id].short_name,
			       pdf_link_obj(dev_font[font_id].font_resource));
    dev_font[font_id].used_on_this_page = 1;
  }
  current_font = font_id;
  return;
}

void dev_set_string (spt_t xpos, spt_t ypos, unsigned char *s, int
		     length, spt_t width, int font_id)
{
  int len = 0;
  long kern;
  if (font_id != current_font)
    dev_set_font(font_id); /* Force a Tf since we are actually trying
			       to write a character */
  /* Kern is in units of character units, i.e., 1000 = 1 em. */
  /* The following formula is of the form a*x/b where a, x, and b are
     long integers.  Since in integer arithmetic (a*x) could overflow
     and a*(x/b) would not be accurate, we use floating point
     arithmetic rather than trying to do this all with integer
     arithmetic. */
  kern =
    (1000.0/dev_font[font_id].extend*(text_xorigin+text_offset-xpos))/dev_font[font_id].sptsize;
  
  if (labs(ypos-text_yorigin) > CENTI_PDF_U || /* CENTI_PDF_U is smallest resolvable dimension */
      abs(kern) > 32000) { /* Some PDF Readers fail on large kerns */
    text_mode();
    kern = 0;
  }
  if (motion_state != STRING_MODE)
    string_mode(xpos, ypos, dev_font[font_id].slant, dev_font[font_id].extend);
  else if (kern != 0) {
    text_offset -=
      kern*dev_font[font_id].extend*(dev_font[font_id].sptsize/1000.0);
    /* Same issues as earlier.  Use floating point for simplicity */
    /* This routine needs to be fast, so we don't call sprintf() or
       strcpy() */
    format_buffer[len++] = ')';
    len += itoa (format_buffer+len, kern);
    format_buffer[len++] = '(';
    pdf_doc_add_to_page (format_buffer, len);
    len = 0;
  }
  len += pdfobj_escape_str (format_buffer+len, FORMAT_BUF_SIZE-len, s,
			    length,
			    dev_font[font_id].remap);
  pdf_doc_add_to_page (format_buffer, len);

  /* Record characters used for partial font embedding */
  /* Fonts without pfbs don't get counted and have used_chars set to
     null */
  if (dev_font[font_id].used_chars != NULL) {
    int i;
    if (dev_font[font_id].remap)
      for (i=0; i<length; i++){
	(dev_font[font_id].used_chars)[twiddle(s[i])] = 1;
      }
    else 
      for (i=0; i<length; i++){
	(dev_font[font_id].used_chars)[s[i]] = 1;
      }
  }
  text_offset += width;
}

void dev_init (double scale, double x_offset, double y_offset)
{
  dvi2pts = scale;
  hoffset = x_offset;
  voffset = y_offset;
  if (debug) fprintf (stderr, "dev_init:\n");
  graphics_mode();
  dev_clear_color_stack();
  dev_clear_xform_stack();
}

void dev_close (void)
{
  /* Set page origin now that user has had plenty of time
     to set page size */
  pdf_doc_set_origin((double) hoffset, (double)
		     dev_page_height()-voffset);
}

void dev_add_comment (char *comment)
{
  pdf_doc_creator (comment);
}


/*  BOP, EOP, and FONT section.
   BOP and EOP manipulate some of the same data structures
   as the font stuff */ 

#define GRAY 1
#define RGB 2
#define CMYK 3
struct color {
  int colortype;
  double c1, c2, c3, c4;
} colorstack[MAX_COLORS], background = {GRAY, 1.0, 1.0, 1.0, 1.0},
    default_color = {GRAY, 0.0, 0.0, 0.0, 0.0};

#include "colors.h"

struct color color_by_name (char *s) 
{
  int i;
  struct color result;
  for (i=0; i<sizeof(colors_by_name)/sizeof(colors_by_name[0]); i++) {
    if (!strcmp (s, colors_by_name[i].name)) {
      break;
    }
  }
  if (i == sizeof(colors_by_name)/sizeof(colors_by_name[0])) {
    fprintf (stderr, "Color \"%s\" no known.  Using \"Black\" instead.\n", s);
    result = default_color;
  } else {
    result = colors_by_name[i].color;
  }
  return result;
}

static int num_colors = 0;

static void fill_page (void)
{
  if (background.colortype == GRAY && background.c1 == 1.0)
    return;
  switch (background.colortype) {
  case GRAY:
    sprintf (format_buffer, " q 0 w %.3f g %.3f G", background.c1, background.c1);
    break;
  case RGB:
    sprintf (format_buffer, " q 0 w %.3f %.3f %.3f rg %.3f %.3f %.3f RG",
	     background.c1, background.c2, background.c3,
	     background.c1, background.c2, background.c3);
    break;
  case CMYK:
    sprintf (format_buffer, " q 0 w %.3f %.3f %.3f %.3f k %.3f %.3f %.3f %.3f K ",
	     background.c1, background.c2, background.c3, background.c4,
	     background.c1, background.c2, background.c3, background.c4);
    break;
  }
  pdf_doc_this_bop (format_buffer, strlen(format_buffer));
  sprintf (format_buffer,
	   " 0 0 m %.2f 0 l %.2f %.2f l 0 %.2f l b Q ",
	   dev_page_width(), dev_page_width(), dev_page_height(),
	   dev_page_height());
  pdf_doc_this_bop (format_buffer, strlen(format_buffer));
  return;
}

void dev_bg_rgb_color (double r, double g, double b)
{
  background.colortype = RGB;
  background.c1 = r;
  background.c2 = g;
  background.c3 = b;
  fill_page();
  return;
}

void dev_bg_cmyk_color (double c, double m, double y, double k)
{
  background.colortype = CMYK;
  background.c1 = c;
  background.c2 = m;
  background.c3 = y;
  background.c4 = k;
  fill_page();
  return;
}

void dev_bg_gray (double value)
{
  background.colortype = GRAY;
  background.c1 = value;
  fill_page();
  return;
}

void dev_bg_named_color (char *s)
{
  struct color color = color_by_name (s);
  switch (color.colortype) {
  case GRAY:
    dev_bg_gray (color.c1);
    break;
  case RGB:
    dev_bg_rgb_color (color.c1, color.c2, color.c3);
    break;
  case CMYK:
    dev_bg_cmyk_color (color.c1, color.c2, color.c3, color.c4);
    break;
  }
  return;
}

static void dev_clear_color_stack (void)
{
  num_colors = 0;
  return;
}
static void dev_set_color (struct color color)
{
  switch (color.colortype) {
    int len;
  case RGB:
    len = sprintf (format_buffer, " %.2f %.2f %.2f",
		   color.c1,
		   color.c2,
		   color.c3);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" rg", 3);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" RG", 3);
    break;
  case CMYK:
    len = sprintf (format_buffer, " %.2f %.2f %.2f %.2f",
		   color.c1,
		   color.c2,
		   color.c3,
		   color.c4);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" k", 2);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" K ", 3);
    break;
  case GRAY:
    len = sprintf (format_buffer, " %.2f", color.c1);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" g", 2);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" G", 2);
    break;
  default:
    ERROR ("Internal error: Invalid default color item");
  }
}


void dev_do_color (void) 
{
  if (num_colors == 0) {
    dev_set_color (default_color);
  } else {
    dev_set_color (colorstack[num_colors-1]);
  }
  return;
}

void dev_set_def_rgb_color (double r, double g, double b)
{
  default_color.c1 = r;
  default_color.c2 = g;
  default_color.c3 = b;
  default_color.colortype = RGB;
  dev_do_color();
  return;
}

void dev_set_def_gray (double g) 
{
  default_color.c1 = g;
  default_color.colortype = GRAY;
  dev_do_color();
  return;
}

void dev_set_def_named_color (char *s)
{
  struct color color = color_by_name (s);
  switch (color.colortype) {
  case GRAY:
    dev_set_def_gray (color.c1);
    break;
  case RGB:
    dev_set_def_rgb_color (color.c1, color.c2, color.c3);
    break;
  case CMYK:
    dev_set_def_cmyk_color (color.c1, color.c2, color.c3, color.c4);
    break;
  }
  return;
}

void dev_set_def_cmyk_color (double c, double m, double y, double k)
{
  default_color.c1 = c;
  default_color.c2 = m;
  default_color.c3 = y;
  default_color.c4 = k;
  default_color.colortype = CMYK;
  dev_do_color();
  return;
}

void dev_begin_named_color (char *s)
{
  struct color color = color_by_name (s);
  switch (color.colortype) {
  case GRAY:
    dev_begin_gray (color.c1);
    break;
  case RGB:
    dev_begin_rgb_color (color.c1, color.c2, color.c3);
    break;
  case CMYK:
    dev_begin_cmyk_color (color.c1, color.c2, color.c3, color.c4);
    break;
  }
  return;
}

void dev_begin_rgb_color (double r, double g, double b)
{
  if (num_colors >= MAX_COLORS) {
    fprintf (stderr, "\ndev_set_color:  Exceeded depth of color stack\n");
    return;
  }
  colorstack[num_colors].c1 = r;
  colorstack[num_colors].c2 = g;
  colorstack[num_colors].c3 = b;
  colorstack[num_colors].colortype = RGB;
  num_colors+= 1;
  dev_do_color();
}

void dev_begin_cmyk_color (double c, double m, double y, double k)
{
  if (num_colors >= MAX_COLORS) {
    fprintf (stderr, "\ndev_set_color:  Exceeded depth of color stack\n");
    return;
  }
  colorstack[num_colors].c1 = c;
  colorstack[num_colors].c2 = m;
  colorstack[num_colors].c3 = y;
  colorstack[num_colors].c4 = k;
  colorstack[num_colors].colortype = CMYK;
  num_colors+= 1;
  dev_do_color();
}

void dev_begin_gray (double value)
{
  if (num_colors >= MAX_COLORS) {
    fprintf (stderr, "\ndev_begin_gray:  Exceeded depth of color stack\n");
    return;
  }
  colorstack[num_colors].c1 = value;
  colorstack[num_colors].colortype = GRAY;
  num_colors+= 1;
  dev_do_color();
}

void dev_end_color (void)
{
  if (num_colors <= 0) {
    fprintf (stderr, "\ndev_set_color:  End color with no corresponding begin color\n");
    return;
  }
  num_colors -= 1;
  dev_do_color();
}

static int num_transforms = 0;

static void dev_clear_xform_stack (void)
{
  num_transforms = 0;
  return;
}

void dev_begin_xform (double xscale, double yscale, double rotate,
		      double x_user, double y_user)
{
  double c, s;
  if (num_transforms >= MAX_TRANSFORMS) {
    fprintf (stderr, "\ndev_begin_xform:  Exceeded depth of transformation stack\n");
    return;
  }
  c = ROUND (cos(rotate),1e-5);
  s = ROUND (sin(rotate),1e-5);
  sprintf (work_buffer, " q %g %g %g %g %.2f %.2f cm",
	   xscale*c, xscale*s, -yscale*s, yscale*c,
	   (1.0-xscale*c)*x_user+yscale*s*y_user,
	   -xscale*s*x_user+(1.0-yscale*c)*y_user);
  pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
  num_transforms += 1;
  return;
}

void dev_end_xform (void)
{
  if (num_transforms <= 0) {
    fprintf (stderr, "\ndev_end_xform:  End transform with no corresponding begin\n");
    return;
  }
  pdf_doc_add_to_page (" Q", 2);
  num_transforms -= 1;
  /* Unfortunately, the following two lines are necessary in case of a font or color
     change inside of the save/restore pair.  Anything that was done
     there must be redone, so in effect, we make no assumptions about
     what fonts. We act like we are starting a new page */
  dev_reselect_font();
  dev_do_color();
  return;
}

int dev_xform_depth (void)
{
  return num_transforms;
}

void dev_close_all_xforms (int depth)
{
  if (num_transforms > depth) {
    fprintf (stderr, "\nspecial: Closing pending transformations at end of page/XObject\n");
    while (num_transforms > depth) {
      num_transforms -= 1;
      pdf_doc_add_to_page (" Q", 2);
    }
    dev_reselect_font();
    dev_do_color();
  }
  return;
}


/* The following routine is here for forms.  Since
   a form is self-contained, it will need its own Tf command
   at the beginningg even if it is continuing to set type
   in the current font.  This routine simply forces reinstantiation
   of the current font. */
void dev_reselect_font(void)
{
  int i;
  current_font = -1;
  for (i=0; i<n_dev_fonts; i++) {
    dev_font[i].used_on_this_page = 0;
  }
  text_slant = 0.0;
  text_extend = 1.0;
}

static void bop_font_reset(void)
{
  dev_reselect_font();
}

void dev_bop (void)
{
#ifdef MEM_DEBUG
MEM_START
#endif
  if (debug) {
    fprintf (stderr, "dev_bop:\n");
  }
  pdf_doc_new_page ();
  fill_page();
  graphics_mode();
  {
    text_slant = 0.0;
    text_extend = 1.0;
  }
  bop_font_reset();
  /* This shouldn't be necessary because line widths are now
     explicitly set for each rule */
  /*  pdf_doc_add_to_page ("0 w", 3); */
  dev_do_color(); /* Set text color in case it was changed on last
		     page */
#ifdef MEM_DEBUG
MEM_END
#endif
}

void dev_eop (void)
{
#ifdef MEM_DEBUG
MEM_START
#endif
  if (debug) {
    fprintf (stderr, "dev_eop:\n");
  }
  graphics_mode();
  dev_close_all_xforms(0);
  pdf_doc_finish_page ();
  /* Finish any pending PS specials */
  mp_eop_cleanup();
#ifdef MEM_DEBUG
MEM_END
#endif
}

static int locate_type1_font (char *tex_name, spt_t ptsize)
     /* Here, the ptsize is in device units, currently millipts */
{
  /* Since Postscript fonts are scaleable, this font may have already
     been asked for in some other point size.  Make sure it doesn't already exist. */
  int i, thisfont;
  if (debug) {
    fprintf (stderr, "locate_type1_font: fontname: (%s) ptsize: %ld, id: %d\n",
	     tex_name, ptsize, n_dev_fonts);
  }
  if (ptsize == 0)
    ERROR ("dev_locate_font called with point size of zero");
  need_more_dev_fonts(1);
  thisfont = n_dev_fonts; /* Use a local variable to handle recursion */
  for (i=0; i<thisfont; i++) {
    if (dev_font[i].format == TYPE1 &&
	dev_font[i].tex_name && strcmp (tex_name, dev_font[i].tex_name) == 0) {
      break;
    }
  }
  if (i == thisfont) {  /* Font *name* not found, so load it and give it a
			    new short name */
    int type1_id = -1;
    dev_font[thisfont].tfm_font_id = tfm_open (tex_name);
    dev_font[thisfont].short_name[0] = 'F';
    sprintf (dev_font[thisfont].short_name+1, "%d", n_phys_fonts+1);
    type1_id = type1_font (tex_name, 
			   dev_font[thisfont].tfm_font_id,
			   dev_font[thisfont].short_name);
    /* type1_font_resource on next line always returns an *indirect* obj */ 
    if (type1_id >= 0) { /* If we got one, it must be a physical font */
      dev_font[thisfont].font_resource = type1_font_resource (type1_id);
      dev_font[thisfont].used_chars = type1_font_used (type1_id);
      dev_font[thisfont].slant = type1_font_slant (type1_id);
      dev_font[thisfont].extend = type1_font_extend (type1_id);
      dev_font[thisfont].remap = type1_font_remap (type1_id);
      n_phys_fonts +=1 ;
    } else { /* No physical font corresponding to this name */
      dev_font[thisfont].short_name[0] = 0;
      thisfont = -1;
    }
  } else {	/* Font name was already in table, presumably (but not
		   necessarily with a
		   different point size */
    /* Copy the parts that do not depend on point size */
    /* Rebuild everything else */
    dev_font[thisfont].tfm_font_id = dev_font[i].tfm_font_id;
    dev_font[thisfont].used_chars = dev_font[i].used_chars;
    dev_font[thisfont].slant = dev_font[i].slant;
    dev_font[thisfont].extend = dev_font[i].extend;
    dev_font[thisfont].remap = dev_font[i].remap;
    strcpy (dev_font[thisfont].short_name, dev_font[i].short_name);
    dev_font[thisfont].font_resource = pdf_link_obj
      (dev_font[i].font_resource);
  }
  /* Note that two entries in dev_font may have the same name and
     pointsize.  This only comes into play with virtual fonts. 
     For example, if the dvi file asks for cmr10 at 12pt and
     one or more vf files also ask for cmr10 at 12pt, multiple entries
     will be generated in the font table.  Names are unique,
     so any given name will only be embedded once. The code to make
     name/ptsize combinations also unique isn't worth the
     effort for the slight increase in storage requirements. */

  if (thisfont >=0) {
    dev_font[thisfont].format = TYPE1; /* We added one */
    dev_font[thisfont].sptsize = ptsize;
    dev_font[thisfont].tex_name = NEW (strlen(tex_name)+1, char);
    strcpy (dev_font[thisfont].tex_name, tex_name);
    /* The value in used_on_this_page is incorrect if the font has
       already been used on the page in a different point size.  It's
       too hard to do right.  The only side effect is that the
       font name will be added to the page resource dict twice.
       Since only one name can exist in the dict, it really makes no
       difference. */
    dev_font[thisfont].used_on_this_page = 0;
    n_dev_fonts +=1;
  }
  return (thisfont);
}

static int locate_pk_font (char *tex_name, spt_t ptsize)
     /* Here, the ptsize is in device units, currently millipts */
{
  /* This routine is different than that for Type 1 fonts.  PK fonts
     are assumed not to be scaleable and so the same name at a
     different point size should be treated as a separate font */
  int i, thisfont;
  if (debug) {
    fprintf (stderr, "locate_pk_font: fontname: (%s) ptsize: %ld, id: %d\n",
	     tex_name, ptsize, n_dev_fonts);
  }
  if (ptsize == 0)
    ERROR ("locate_pk_font called with point size of zero");
  thisfont = n_dev_fonts;
  for (i=0; i<thisfont; i++) {
    /* For pk fonts, both name *and* ptsize must match */
    if (dev_font[i].format == PK &&
	dev_font[i].tex_name && strcmp (tex_name,
					dev_font[i].tex_name) == 0 &&
	dev_font[i].sptsize == ptsize) {
      break;
    }
  }
  if (i == thisfont) {  /* Font *name* not found, so load it and give it a
			    new short name */
    int pk_id = -1;
    need_more_dev_fonts (1);
    dev_font[thisfont].tfm_font_id = tfm_open (tex_name);
    dev_font[thisfont].short_name[0] = 'F';
    itoa (dev_font[thisfont].short_name+1, n_phys_fonts+1);
    pk_id = pk_font (tex_name, ptsize*dvi2pts,
		     dev_font[thisfont].tfm_font_id,
		     dev_font[thisfont].short_name);
    /* type1_font_resource on next line always returns an *indirect* obj */ 
    if (pk_id >= 0) { /* If we got one, it must be a physical font */
      dev_font[thisfont].format = PK;
      dev_font[thisfont].font_resource = pk_font_resource (pk_id);
      dev_font[thisfont].used_chars = pk_font_used (pk_id);
      /* Don't set extend, slant, or remap for PK fonts for now... */
      dev_font[thisfont].slant = 0.0;
      dev_font[thisfont].extend = 1.0;
      dev_font[thisfont].remap = 0;
      dev_font[thisfont].sptsize = ptsize;
      dev_font[thisfont].tex_name = NEW (strlen(tex_name)+1, char);
      dev_font[thisfont].used_on_this_page = 0;
      strcpy (dev_font[thisfont].tex_name, tex_name);
      n_dev_fonts +=1; /* For non rescaleable fonts, dev_fonts and
			  phys_fonts have the same number */
      n_phys_fonts +=1 ;
    } else { /* No physical font corresponding to this name */
      dev_font[thisfont].short_name[0] = 0;
      thisfont = -1;
    }
  } else {
    thisfont = i;
  }
  return (thisfont);
}

int dev_locate_font (char *tex_name, spt_t ptsize)
{
  int result;
  /* If there's a type1 font with this name use it */
  if (debug)
    fprintf (stderr, "\ndev_locate_font(%s, %ld)\n", tex_name, ptsize);
  result = locate_type1_font (tex_name, ptsize);
  /* If not, try to find a pk font */
  if (result < 0) {
    fprintf (stderr, "\nUnable to locate a Type 1 font for (%s)... Hope that's okay.\n",
	     tex_name);
    result = locate_pk_font (tex_name, ptsize);
  }
  /* Otherwise we are out of luck */
  return result;
}


int dev_font_tfm (int dev_font_id)
{
  return dev_font[dev_font_id].tfm_font_id;
}

spt_t dev_font_sptsize (int dev_font_id)
{
  return dev_font[dev_font_id].sptsize;
}

void dev_close_all_fonts(void)
{
  int i;
  for (i=0; i<n_dev_fonts; i++) {
    pdf_release_obj (dev_font[i].font_resource);
    RELEASE (dev_font[i].tex_name);
  }
  if (dev_font)
    RELEASE (dev_font);
  type1_close_all();
  pk_close_all();
}

void dev_rule (spt_t xpos, spt_t ypos, spt_t width, spt_t height)
{
  int len = 0;
  long w, p1, p2, p3, p4;
  graphics_mode();
   /* Is using a real stroke the right thing to do?  It seems to preserve
      the logical meaning of a "rule" as opposed to a filled rectangle.
      I am assume the reader can more intelligently render a rule than a filled rectangle */
  if (width> height) {  /* Horizontal stroke? */
    spt_t half_height = height/2;
    w = height / CENTI_PDF_U;
    p1 = xpos / CENTI_PDF_U;
    p2 = (ypos+half_height) / CENTI_PDF_U;
    p3 = (xpos+width) / CENTI_PDF_U;
    p4 = (ypos+half_height) / CENTI_PDF_U;
  } else { /* Vertical stroke */
    spt_t half_width = width/2;
    w = width / CENTI_PDF_U;
    p1 = (xpos+half_width) / CENTI_PDF_U;
    p2 = ypos / CENTI_PDF_U;
    p3 = (xpos+half_width) / CENTI_PDF_U;
    p4 = (ypos+height) / CENTI_PDF_U;
  }
  /* This needs to be quick */
  {
    format_buffer[len++] = ' ';
    len += centi_u_to_a (format_buffer+len, w);
    format_buffer[len++] = ' ';
    format_buffer[len++] = 'w';
    format_buffer[len++] = ' ';
    len += centi_u_to_a (format_buffer+len, p1);
    format_buffer[len++] = ' ';
    len += centi_u_to_a (format_buffer+len, p2);
    format_buffer[len++] = ' ';
    format_buffer[len++] = 'm';
    format_buffer[len++] = ' ';
    len += centi_u_to_a (format_buffer+len, p3);
    format_buffer[len++] = ' ';
    len += centi_u_to_a (format_buffer+len, p4);
    format_buffer[len++] = ' ';
    format_buffer[len++] = 'l';
    format_buffer[len++] = ' ';
    format_buffer[len++] = 'S';
  }
  pdf_doc_add_to_page (format_buffer, len);
}

/* The following routines tell the coordinates in physical PDF style
   coordinate with origin at bottom left of page.  All other
   coordinates in this routine are in TeX style coordinates */

double dev_phys_x (void)
{
  return dvi_dev_xpos()*dvi_tell_mag() + hoffset;
}

double dev_phys_y (void)
{
  return dev_page_height() + dvi_tell_mag()*dvi_dev_ypos() -voffset;
}

void dev_do_special (void *buffer, UNSIGNED_QUAD size, double x_user,
		     double y_user)
{
  graphics_mode();
  if (!pdf_parse_special (buffer, size, x_user, y_user) &&
      !tpic_parse_special (buffer, size, x_user, y_user) &&
      !htex_parse_special (buffer, size) &&
      !color_special (buffer, size) &&
      !ps_parse_special (buffer, size, x_user, y_user)) {
    fprintf (stderr, "\nUnrecognized special ignored\n");
    dump (buffer, ((char *)buffer)+size);
  }
}

static unsigned dvi_stack_depth = 0;
static int dvi_tagged_depth = -1;
static unsigned char link_annot = 1;
void dev_link_annot (unsigned char flag)
{
  link_annot = flag;
}

void dev_stack_depth (unsigned int depth)
{
  /* If decreasing below tagged_depth */
  if (link_annot && 
      dvi_stack_depth == dvi_tagged_depth &&
      depth == dvi_tagged_depth - 1) {
  /* See if this appears to be the end of a "logical unit"
     that's been broken.  If so, flush the logical unit */
    pdf_doc_flush_annot();
  }
  dvi_stack_depth = depth;
  return;
}

/* The following routines setup and tear down a callback at
   a certain stack depth.  This is used to handle broken (linewise)
   links */

void dev_tag_depth (void)
{
  dvi_tagged_depth = dvi_stack_depth;
  dvi_compute_boxes (1);
  return;
}

void dev_untag_depth (void)
{
  dvi_tagged_depth = -1;
  dvi_compute_boxes (0);
  return;
}

void dev_expand_box (spt_t width, spt_t height, spt_t depth)
{
  double phys_width, phys_height, phys_depth, scale;
  if (link_annot && dvi_stack_depth >= dvi_tagged_depth) {
    scale = dvi2pts*dvi_tell_mag();
    phys_width = scale*width;
    phys_height = scale*height;
    phys_depth = scale*depth;
    pdf_doc_expand_box (dev_phys_x(), dev_phys_y()-phys_depth,
			dev_phys_x()+phys_width,
			dev_phys_y()+phys_height);
  }
}













