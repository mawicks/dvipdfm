/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdev.c,v 1.3 1998/11/30 20:38:25 mwicks Exp $

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

	
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "pdfdev.h"
#include "pdfdoc.h"
#include "pdfobj.h"
#include "error.h"
#include "numbers.h"
#include "type1.h"
#include "mem.h"
#include "io.h"
#include "pdfspecial.h"
#include "pdflimits.h"

/* Internal functions */
static void dev_clear_color_stack (void);
static void dev_clear_xform_stack (void);

#define HOFFSET 72.0
#define VOFFSET 72.0
#define DPI 72u 
 /* Acrobat doesn't seem to like coordinate systems
    that involve scalings around 0.01, so we use
    a scaline of 1.0.  In other words, device units = pts */ 
/* Following dimensions in virtual device coordinates,
which are points */

static double page_width=612.0, page_height=792.0;

void dev_set_page_size (double width, double height)
{
  page_width = width;
  page_height = height;
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

unsigned long dev_tell_xdpi(void)
{
  return DPI;
}

unsigned long dev_tell_ydpi(void)
{
  return DPI;
}

int motion_state = 1; /* Start in graphics mode */

#define GRAPHICS_MODE 1
#define TEXT_MODE 2
#define STRING_MODE 3
#define LINE_MODE 4
#define NO_MODE -1

static char format_buffer[256];
 /* Device coordinates are relative to upper left of page.  One of the
    first things appearing on the page is a coordinate transformation
    matrix that forces this to be true.  This coordinate
    transformation is the only place where the paper size is required.
    Unfortunately, positive is up, which doesn't agree with TeX's convention.  */

static double dev_xpos, dev_ypos; 


int n_dev_fonts = 0;
int current_font = -1;
double current_ptsize = 1.0;
static pdf_obj *this_page_fontlist_dict;

#define MAX_DEVICE_FONTS 256

struct dev_font {
  char short_name[7];	/* Needs to be big enough to hold name "Fxxx"
			   where xxx is number of largest font */
  char *tex_name;
  char used_on_this_page;
  long tex_font_id;
  double ptsize;
  pdf_obj *font_resource;
} dev_font[MAX_DEVICE_FONTS];

static void text_mode (void)
{
  switch (motion_state) {
  case NO_MODE:
  case GRAPHICS_MODE:
    pdf_doc_add_to_page (" BT ", 4);
    /* Following may be necessary after a rule (and also after
       specials) */
    if (current_font != -1) {
      sprintf (format_buffer, " /%s %g Tf ", dev_font[current_font].short_name,
	       ROUND(dev_font[current_font].ptsize*DPI/72,0.01));
      pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    }
    break;
  case STRING_MODE:
    pdf_doc_add_to_page (")", 1);  /*  Fall through */
  case LINE_MODE:
    pdf_doc_add_to_page ("]TJ ", 4);
    break;
  }
  motion_state = TEXT_MODE;
}

static void graphics_mode (void)
{
  switch (motion_state) {
  case STRING_MODE:
    pdf_doc_add_to_page (")", 1); /* Fall through */
  case LINE_MODE:
    pdf_doc_add_to_page ("]TJ ", 4); /* Fall through */
  case TEXT_MODE:
    pdf_doc_add_to_page (" ET ", 4);
    break;
  }
  motion_state = GRAPHICS_MODE;
}

static void string_mode (void)
{
 switch (motion_state) {
 case NO_MODE:
 case GRAPHICS_MODE:
   pdf_doc_add_to_page (" BT ", 4); /* Fall through */
    /* Following may be necessary after a rule (and also after
       specials) */
   if (current_font != -1) {
     sprintf (format_buffer, " /%s %g Tf ", dev_font[current_font].short_name,
	      ROUND(dev_font[current_font].ptsize*DPI/72,0.01));
     pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
   }
 case TEXT_MODE:
   sprintf (format_buffer, "1 0 0 1 %g %g Tm [",
	    ROUND(dev_xpos,0.01), ROUND(dev_ypos,0.01));
   pdf_doc_add_to_page (format_buffer, strlen(format_buffer)); /* Fall
								  through */
 case LINE_MODE:
   pdf_doc_add_to_page ("(", 1);
   break;
 }
 motion_state = STRING_MODE;
}

static void line_mode (void)
{
 switch (motion_state) {
 case NO_MODE:
 case GRAPHICS_MODE:
   pdf_doc_add_to_page (" BT ", 4); /* Fall through */
    /* Following may be necessary after a rule (and also after
       specials) */
   if (current_font != -1) {
     sprintf (format_buffer, " /%s %g Tf ", dev_font[current_font].short_name,
	      ROUND(dev_font[current_font].ptsize*DPI/72,0.01));
     pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
   }
 case TEXT_MODE:
   sprintf (format_buffer, "1 0 0 1 %g %g Td [",
	    ROUND(dev_xpos,0.01), ROUND(dev_ypos,0.01));
   pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
   break;
 case STRING_MODE:
   pdf_doc_add_to_page (")", 1);
   break;
 }
 motion_state = LINE_MODE;
}

void dev_init (char *outputfile)
{
  if (debug) fprintf (stderr, "dev_init:\n");
  pdf_doc_init (outputfile);
  graphics_mode();
  dev_clear_color_stack();
  dev_clear_xform_stack();
}

void dev_add_comment (char *comment)
{
  pdf_doc_creator (comment);
}


void dev_close (void)
{
  if (debug) fprintf (stderr, "dev_close:\n");
  graphics_mode();
  pdf_finish_specials();
  pdf_doc_finish ();
}


/*  BOP, EOP, and FONT section.
   BOP and EOP manipulate some of the same data structures
   as the font stuff */ 

static void bop_font_reset(void)
{
  int i;
  for (i=0; i<n_dev_fonts; i++) {
    dev_font[i].used_on_this_page = 0;
  }
  current_font = -1;
}

#define GRAY 1
#define RGB 2
#define CMYK 3
struct color {
  int colortype;
  double c1, c2, c3, c4;
} colorstack[MAX_COLORS], background = {GRAY, 1.0, 1.0, 1.0, 1.0};

static int num_colors = 0;

static void fill_page (void)
{
  if (background.colortype == GRAY && background.c1 == 1.0)
    return;
  switch (background.colortype) {
  case GRAY:
    sprintf (format_buffer, " q 0 w %g g %g G", background.c1, background.c1);
    break;
  case RGB:
    sprintf (format_buffer, " q 0 w %g %g %g rg %g %g %g RG",
	     background.c1, background.c2, background.c3,
	     background.c1, background.c2, background.c3);
    break;
  case CMYK:
    sprintf (format_buffer, " q 0 w %g %g %g %g k %g %g %g %g K",
	     background.c1, background.c2, background.c3, background.c4,
	     background.c1, background.c2, background.c3, background.c4);
    break;
  }
  pdf_doc_this_bop (format_buffer, strlen(format_buffer));
  sprintf (format_buffer,
	   " 0 0 m %g 0 l %g %g l 0 %g l b Q",
	   page_width, page_width, page_height, page_height);
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

static void dev_clear_color_stack (void)
{
  num_colors = 0;
  return;
}

static void dev_do_color (void) 
{
  if (num_colors == 0) {
    pdf_doc_add_to_page (" 0 g 0 G ", 9);
    return;
  }
  switch (colorstack[num_colors-1].colortype) {
  case RGB:
    sprintf (format_buffer, " %g %g %g",
	     colorstack[num_colors-1].c1,
	     colorstack[num_colors-1].c2,
	     colorstack[num_colors-1].c3);
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    pdf_doc_add_to_page (" rg", 3);
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    pdf_doc_add_to_page (" RG ", 4);
    break;
  case CMYK:
    sprintf (format_buffer, " %g %g %g %g ",
	     colorstack[num_colors-1].c1,
	     colorstack[num_colors-1].c2,
	     colorstack[num_colors-1].c3,
	     colorstack[num_colors-1].c4);
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    pdf_doc_add_to_page (" k", 2);
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    pdf_doc_add_to_page (" K ", 3);
    break;
  case GRAY:
    sprintf (format_buffer, " %g", colorstack[num_colors-1].c1);
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    pdf_doc_add_to_page (" g", 2);
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    pdf_doc_add_to_page (" G ", 3);
    break;
  default:
    ERROR ("Internal error: Invalid color item on color stack");
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
  colorstack[num_colors].c4 = y;
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

void dev_begin_xform (double xscale, double yscale, double rotate)
{
  double c, s;
  if (num_transforms >= MAX_TRANSFORMS) {
    fprintf (stderr, "\ndev_set_color:  Exceeded depth of transformation stack\n");
    return;
  }
  c = ROUND (cos(rotate),1e-5);
  s = ROUND (sin(rotate),1e-5);
  sprintf (work_buffer, " q %g %g %g %g %g %g cm ",
	   xscale*c, xscale*s, -yscale*s, yscale*c,
	   ROUND((1.0-xscale*c)*dev_xpos+yscale*s*dev_ypos,0.001),
	   ROUND(-xscale*s*dev_xpos+(1.0-yscale*c)*dev_ypos,0.001));
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
  pdf_doc_add_to_page (" Q ", 3);
  num_transforms -= 1;
  return;
}

static void dev_close_all_xforms (void)
{
  if (num_transforms)
    fprintf (stderr, "\nspecial: Closing pending transformations at end of page\n");
  while (num_transforms > 0) {
    num_transforms -= 1;
    pdf_doc_add_to_page (" Q ", 3);
  }
  return;
}


void dev_bop (void)
{
  if (debug) {
    fprintf (stderr, "dev_bop:\n");
  }
    
  pdf_doc_new_page (page_width, page_height);
  fill_page();
  graphics_mode();
  dev_xpos = 0.0;
  dev_ypos = 0.0;
  sprintf (format_buffer, "%g 0 0 %g 0 0 cm ", ROUND(72.0/DPI,0.001), ROUND(72.0/DPI,0.001));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  sprintf (format_buffer, "1 0 0 1 0 %ld cm ", (long) page_height);
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  this_page_fontlist_dict = pdf_new_dict ();
  bop_font_reset();
  pdf_doc_add_to_page (" 0 w ", 5);
  dev_do_color();
}

void dev_eop (void)
{
  if (debug) {
    fprintf (stderr, "dev_eop:\n");
  }
  graphics_mode();
  dev_close_all_xforms();
  pdf_doc_add_to_page_resources ("Font", this_page_fontlist_dict);
}

void dev_locate_font (char *tex_name,
		      unsigned long tex_font_id,
		      int tfm_font_id,
		      double ptsize)
{
  /* Since Postscript fonts are scaleable, this font may have already
     been asked for.  Make sure it doesn't already exist. */
  int i;
  if (debug) {
    fprintf (stderr, "dev_locate_font:\n");
  }
  if (n_dev_fonts == MAX_DEVICE_FONTS)
    ERROR ("dev_locate_font:  Tried to load too many fonts\n");
  for (i=0; i<n_dev_fonts; i++) {
    if (strcmp (tex_name, dev_font[i].tex_name) == 0) {
      break;
    }
  }
  if (i == n_dev_fonts) {  /* Font not found, so load it */
    dev_font[i].short_name[0] = 'F';
    sprintf (dev_font[i].short_name+1, "%d", n_dev_fonts+1);
    /* type1_font_resource on next line always returns an indirect
       reference */
    dev_font[i].font_resource = type1_font_resource (tex_name,
						     tfm_font_id,
						     dev_font[i].short_name);
  } else {	/* Font was found */
    strcpy (dev_font[n_dev_fonts].short_name, dev_font[i].short_name);
    /* Next line makes a copy of an indirect object.  It's not really the
       indirect object pointing to an indirect object that it looks
       like.  Someday a true object copy constructor needs to be
       written */
    dev_font[n_dev_fonts].font_resource = pdf_ref_obj (dev_font[i].font_resource);
  }
  dev_font[n_dev_fonts].tex_font_id = tex_font_id;
  dev_font[n_dev_fonts].ptsize = ptsize;
  dev_font[n_dev_fonts].used_on_this_page = 0;
  dev_font[n_dev_fonts].tex_name = NEW (strlen(tex_name)+1, char);
  strcpy (dev_font[n_dev_fonts].tex_name, tex_name);
  n_dev_fonts += 1;
}

void dev_select_font (long tex_font_id)
{
  int i;
  if (debug) {
    fprintf (stderr, "(dev_select_font)");
  }
  for (i=0; i<n_dev_fonts; i++) {
    if (dev_font[i].tex_font_id == tex_font_id)
      break;
  }
  if (i == n_dev_fonts) {
    ERROR ("dev_change_to_font:  dvi wants a font that isn't loaded");
  }
  text_mode();
  sprintf (format_buffer, " /%s %g Tf ", dev_font[i].short_name,
	   ROUND(dev_font[i].ptsize*DPI/72,0.01));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  current_font = i;
  current_ptsize = dev_font[i].ptsize;
  /* Add to resource list for this page */
  if (!dev_font[i].used_on_this_page) {
    pdf_add_dict (this_page_fontlist_dict,
		  pdf_new_name (dev_font[i].short_name),
		  pdf_link_obj (dev_font[i].font_resource));
    dev_font[i].used_on_this_page = 1;
  }
}

void dev_set_char (unsigned ch, double width)
{
  int len;
  if (debug) {
    fprintf (stderr, "(dev_set_char (width=%g)", width);
    if (isprint (ch))
      fprintf (stderr, "(%c)", ch);
    fprintf (stderr, ")");
  }
  string_mode();
  len = pdfobj_escape_c (format_buffer, ch);
  pdf_doc_add_to_page (format_buffer, len);
  dev_xpos += width;
}

void dev_rule (double width, double height)
{
  if (debug) {
    fprintf (stderr, "(dev_rule)");
  }
  graphics_mode();
  sprintf (format_buffer, "%g %g m %g %g l %g %g l %g %g l b ",
	   ROUND(dev_xpos,0.01), ROUND(dev_ypos,0.01),
	   ROUND(dev_xpos+width,0.01), ROUND(dev_ypos,0.01),
	   ROUND(dev_xpos+width,0.01), ROUND(dev_ypos+height,0.01),
	   ROUND(dev_xpos,0.01), ROUND(dev_ypos+height,0.01));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
}

void dev_moveright (double x)
{
  if (debug) {
    fprintf (stderr, "(dev_moveright %g)", x);
  }
  /* This moveright is only for text (not rules) 
     No sense doing this unless in some kind of text mode already.
     In other words, don't go enter LINE or STRING mode just
     to do this. The reason is a moveright in graphics mode isn't
     easy to implement in PDF.  The driver should always
     do an absolute dev_moveto before a rule */

  /* Acrobat reader apparently can't handle large relative motions.
     Relative motions are used for kerning and interword spacing.
     If a relative motion is too large, we force textmode() to
     change it to an absolute motion. */

  if ( fabs(72000.0/current_ptsize*x/DPI) > 1000.0 && 
       (motion_state == LINE_MODE ||
	motion_state == STRING_MODE)) {
    text_mode();
  }
  if (
      motion_state == LINE_MODE ||
      motion_state == STRING_MODE) { 
    line_mode();
    sprintf (format_buffer, "%g", -ROUND(72000.0/current_ptsize*x/DPI,1.0));
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  }
  dev_xpos += x;
}

void dev_moveto (double x, double y)
{
  if (debug) {
    fprintf (stderr, "(dev_moveto)");
  }
  if (motion_state == LINE_MODE ||
      motion_state == STRING_MODE) {
    text_mode();
  }
  dev_xpos = x + HOFFSET;
  dev_ypos = -y - VOFFSET;
}

/* The following routines tell the coordinates in physical PDF style
   coordinate with origin at bottom left of page.  All other
   coordinates in this routine are in TeX style coordinates */

double dev_tell_x (void)
{
  return dev_xpos;
}

double dev_tell_y (void)
{
  return page_height+dev_ypos;
}


void dev_do_special (char *buffer, UNSIGNED_QUAD size)
{
  graphics_mode();
  pdf_parse_special (buffer, size, dev_xpos, dev_ypos, dev_xpos, page_height+dev_ypos);
}
