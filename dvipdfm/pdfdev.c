/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdev.c,v 1.58 1999/01/06 02:26:01 mwicks Exp $

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
#include "mfileio.h"
#include "pdfspecial.h"
#include "pdflimits.h"
#include "tfm.h"
#include "dvi.h"

/* Internal functions */
static void dev_clear_color_stack (void);
static void dev_clear_xform_stack (void);

double hoffset = 72.0, voffset=72.0;
#define DPI 72u 

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

unsigned long dev_tell_xdpi(void)
{
  return DPI;
}

unsigned long dev_tell_ydpi(void)
{
  return DPI;
}

#define GRAPHICS_MODE 1
#define TEXT_MODE 2
#define STRING_MODE 3

int motion_state = GRAPHICS_MODE; /* Start in graphics mode */

#define FORMAT_BUF_SIZE 256
static char format_buffer[FORMAT_BUF_SIZE];

 /* Device coordinates are relative to upper left of page.  One of the
    first things appearing in the page stream is a coordinate transformation
    matrix that forces this to be true.  This coordinate
    transformation is the only place where the paper size is required.
    Unfortunately, positive is up, which doesn't agree with TeX's convention.  */

static mpt_t text_xorigin = 0, text_yorigin = 0,
  text_offset = 0, text_leading = 0;
double text_xerror = 0.0, text_yerror = 0.0;

int n_dev_fonts = 0;
int n_phys_fonts = 0;
int current_font = -1;

#define MAX_DEVICE_FONTS 256

#define PHYSICAL 1
#define VIRTUAL 2

static struct dev_font {
  char short_name[7];	/* Needs to be big enough to hold name "Fxxx"
			   where xxx is number of largest font */
  int used_on_this_page;
  char *tex_name;
  int tfm_font_id;
  double ptsize;
  mpt_t mptsize;
  pdf_obj *font_resource;
  char *used_chars;
} dev_font[MAX_DEVICE_FONTS];

static void reset_text_state(void)
{
  text_xorigin = 0;
  text_yorigin = 0;
  text_leading = 0;
  text_offset = 0;
  text_xerror = 0.0;
  text_yerror = 0.0;
}


static void text_mode (void)
{
  switch (motion_state) {
  case STRING_MODE:
    pdf_doc_add_to_page (")]TJ", 4);
  case TEXT_MODE:
    break;
  case GRAPHICS_MODE:
    pdf_doc_add_to_page (" BT", 3);
    reset_text_state();
    break;
  }
  motion_state = TEXT_MODE;
  return;
}

static void graphics_mode (void)
{
  int len = 0;
  switch (motion_state) {
  case GRAPHICS_MODE:
    break;
  case STRING_MODE:
    len += sprintf (format_buffer+len, ")]TJ"); /* Fall through */
  case TEXT_MODE:
    len += sprintf (format_buffer+len, " ET");
    pdf_doc_add_to_page (format_buffer, len);
    reset_text_state();
    break;
  }
  motion_state = GRAPHICS_MODE;
  return;
}

static void string_mode (mpt_t xpos, mpt_t ypos)
{
  mpt_t delx, dely;
  int len = 0;
  switch (motion_state) {
  case STRING_MODE:
    break;
  case GRAPHICS_MODE:
    len += sprintf (format_buffer+len, " BT"); /* Fall through */
    reset_text_state();
    /* Following may be necessary after a rule (and also after
       specials) */
  case TEXT_MODE:
    delx = xpos - text_xorigin;
    {
      double rounded_delx, desired_delx;
      double rounded_dely, desired_dely;
      desired_delx = delx*dvi2pts+text_xerror;
      rounded_delx = ROUND(desired_delx,0.01);
      text_xerror = desired_delx - rounded_delx;
      dely = ypos - text_yorigin;
      desired_dely = dely*dvi2pts+text_yerror;
      rounded_dely = ROUND(desired_dely,0.01);
      text_yerror = desired_dely - rounded_dely;
      len += sprintf (format_buffer+len, " %.7g %.7g TD[(",
		      rounded_delx, rounded_dely);
      text_leading = dely;
      text_xorigin = xpos;
      text_yorigin = ypos;
    }
    text_offset = 0;
    pdf_doc_add_to_page (format_buffer, len);
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
  len = sprintf (format_buffer, "/%s %.6g Tf", dev_font[font_id].short_name,
		 dev_font[font_id].ptsize);
  pdf_doc_add_to_page (format_buffer, len);
  /* Add to Font list in Resource dictionary for this page */
  if (!dev_font[font_id].used_on_this_page) { 
    pdf_doc_add_to_page_fonts (dev_font[font_id].short_name,
			       pdf_link_obj(dev_font[font_id].font_resource));
    dev_font[font_id].used_on_this_page = 1;
  }
  current_font = font_id;
}

void dev_set_string (mpt_t xpos, mpt_t ypos, unsigned char *s, int
		     length, mpt_t width, int font_id)
{
  int len = 0;
  long kern;
  kern = (1000.0*(text_xorigin+text_offset-xpos))/dev_font[font_id].mptsize;
  if (font_id != current_font)
    dev_set_font(font_id); /* Force a Tf since we are actually trying
			       to write a character */
  else if (ypos != text_yorigin ||
	   abs(kern) > 32000) {
    text_mode();
    kern = 0;
  }
  if (motion_state != STRING_MODE)
    string_mode(xpos, ypos);
  else if (kern != 0) {
    text_offset -= kern*(dev_font[font_id].mptsize/1000.0);
    len += sprintf (format_buffer+len, ")%ld(", kern);
  }
  len += pdfobj_escape_str (format_buffer+len, FORMAT_BUF_SIZE-len, s, length);
  pdf_doc_add_to_page (format_buffer, len);

  /* Record characters used for partial font embedding */
  /* Fonts without pfbs don't get counted and have used_chars set to
     null */
  if (dev_font[font_id].used_chars != NULL) {
    int i;
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
} colorstack[MAX_COLORS], background = {GRAY, 1.0, 1.0, 1.0, 1.0};

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

static void dev_clear_color_stack (void)
{
  num_colors = 0;
  return;
}

void dev_do_color (void) 
{
  int len = 0;
  if (num_colors == 0) {
    pdf_doc_add_to_page (" 0 g 0 G", 8);
    return;
  }
  switch (colorstack[num_colors-1].colortype) {
  case RGB:
    len = sprintf (format_buffer, " %.2f %.2f %.2f",
		   colorstack[num_colors-1].c1,
		   colorstack[num_colors-1].c2,
		   colorstack[num_colors-1].c3);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" rg", 3);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" RG", 3);
    break;
  case CMYK:
    len = sprintf (format_buffer, " %.2f %.2f %.2f %.2f",
		   colorstack[num_colors-1].c1,
		   colorstack[num_colors-1].c2,
		   colorstack[num_colors-1].c3,
		   colorstack[num_colors-1].c4);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" k", 2);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" K ", 3);
    break;
  case GRAY:
    len = sprintf (format_buffer, " %.2f", colorstack[num_colors-1].c1);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" g", 2);
    pdf_doc_add_to_page (format_buffer, len);
    pdf_doc_add_to_page (" G", 2);
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
  return;
}

void dev_close_all_xforms (void)
{
  if (num_transforms)
    fprintf (stderr, "\nspecial: Closing pending transformations at end of page/XObject\n");
  while (num_transforms > 0) {
    num_transforms -= 1;
    pdf_doc_add_to_page (" Q", 2);
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
  bop_font_reset();
  pdf_doc_add_to_page ("0 w", 3);
  dev_do_color();
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
  dev_close_all_xforms();
#ifdef MEM_DEBUG
MEM_END
#endif
}

int dev_locate_font (char *tex_name, mpt_t ptsize)
     /* Here, the ptsize is in device units, currently millipts */
{
  /* Since Postscript fonts are scaleable, this font may have already
     been asked for.  Make sure it doesn't already exist. */
  int i, thisfont;
  if (debug) {
    fprintf (stderr, "dev_locate_font: fontname: (%s) ptsize: %ld, id: %d\n",
	     tex_name, ptsize, n_dev_fonts);
  }
  if (ptsize == 0)
    ERROR ("dev_locate_font called with point size of zero");
  if (n_dev_fonts == MAX_DEVICE_FONTS)
    ERROR ("dev_locate_font:  Tried to load too many fonts\n");
  thisfont = n_dev_fonts;
  for (i=0; i<thisfont; i++) {
    if (dev_font[i].tex_name && strcmp (tex_name, dev_font[i].tex_name) == 0) {
      break;
    }
  }
  if (i == thisfont) {  /* Font *name* not found, so load it and give it a
			    new short name */
    int type1_id = -1;
    dev_font[thisfont].tfm_font_id = tfm_open (tex_name);
    dev_font[i].short_name[0] = 'F';
    sprintf (dev_font[i].short_name+1, "%d", n_phys_fonts+1);
    type1_id = type1_font (tex_name, 
			   dev_font[thisfont].tfm_font_id,
			   dev_font[thisfont].short_name);
    /* type1_font_resource on next line always returns an *indirect* obj */ 
    if (type1_id >= 0) { /* If we got one, it must be a physical font */
      dev_font[thisfont].font_resource = type1_font_resource (type1_id);
      dev_font[thisfont].used_chars = type1_font_used (type1_id);
      n_phys_fonts +=1 ;
    } else { /* No physical font corresponding to this name */
      thisfont = -1;
      dev_font[thisfont].short_name[0] = 0;
    }
  } else {	/* Font name was already in table*/
    /* Copy the parts that do not depend on point size */
    /* Rebuild everything else */
    dev_font[thisfont].tfm_font_id = dev_font[i].tfm_font_id;
    dev_font[thisfont].used_chars = dev_font[i].used_chars;
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
    dev_font[thisfont].mptsize = ptsize;
    dev_font[thisfont].ptsize = ROUND(ptsize*dvi2pts,0.01);
    dev_font[thisfont].tex_name = NEW (strlen(tex_name)+1, char);
    strcpy (dev_font[thisfont].tex_name, tex_name);
    n_dev_fonts +=1;
  }
  return (thisfont);
}

int dev_font_tfm (int dev_font_id)
{
  return dev_font[dev_font_id].tfm_font_id;
}

double dev_font_size (int dev_font_id)
{
  return dev_font[dev_font_id].ptsize;
}

mpt_t dev_font_mptsize (int dev_font_id)
{
  return dev_font[dev_font_id].mptsize;
}

void dev_close_all_fonts(void)
{
  int i;
  for (i=0; i<n_dev_fonts; i++) {
    pdf_release_obj (dev_font[i].font_resource);
    RELEASE (dev_font[i].tex_name);
  }
  type1_close_all();
}


void dev_rule (mpt_t xpos, mpt_t ypos, mpt_t width, mpt_t height)
{
  int len = 0;
  if (debug) {
    fprintf (stderr, "(dev_rule)");
  }
  graphics_mode();
  len = sprintf (format_buffer, " %.2f %.2f m %.2f %.2f l %.2f %.2f l %.2f %.2f l b",
		 xpos*dvi2pts, ypos*dvi2pts,
		 (xpos+width)*dvi2pts, ypos*dvi2pts,
		 (xpos+width)*dvi2pts, (ypos+height)*dvi2pts,
		 xpos*dvi2pts, (ypos+height)*dvi2pts);
  pdf_doc_add_to_page (format_buffer, len);
}

/* The following routines tell the coordinates in physical PDF style
   coordinate with origin at bottom left of page.  All other
   coordinates in this routine are in TeX style coordinates */

double dev_phys_x (void)
{
  return dvi_dev_xpos()+hoffset;
}

double dev_phys_y (void)
{
  return dev_page_height()+dvi_dev_ypos()-voffset;
}


void dev_do_special (void *buffer, UNSIGNED_QUAD size, double x_user,
		     double y_user)
{
  graphics_mode();
  pdf_parse_special (buffer, size, x_user, y_user);
}

