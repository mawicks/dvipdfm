/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdev.c,v 1.29 1998/12/09 21:51:52 mwicks Exp $

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
#include "vf.h"

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

int motion_state = 1; /* Start in graphics mode */

#define GRAPHICS_MODE 1
#define TEXT_MODE 2
#define STRING_MODE 3
#define NO_MODE -1

static char format_buffer[256];
 /* Device coordinates are relative to upper left of page.  One of the
    first things appearing on the page is a coordinate transformation
    matrix that forces this to be true.  This coordinate
    transformation is the only place where the paper size is required.
    Unfortunately, positive is up, which doesn't agree with TeX's convention.  */

static double text_xorigin = 0.0, text_yorigin = 0.0,
  text_offset = 0.0, text_leading =0.0;

int n_dev_fonts = 0;
int n_phys_fonts = 0;
int current_font = -1;
int current_phys_font = -1;
double current_ptsize = 1.0;

#define MAX_DEVICE_FONTS 256

#define PHYSICAL 1
#define VIRTUAL 2
static struct dev_font {
  char short_name[7];	/* Needs to be big enough to hold name "Fxxx"
			   where xxx is number of largest font */
  char *tex_name;
  int tfm_font_id;
  double ptsize;
  pdf_obj *font_resource;
  int type;
  int vf_font_id; /* Only used for type = VIRTUAL */
} dev_font[MAX_DEVICE_FONTS];

static void reset_text_state(void)
{
  text_xorigin = 0.0;
  text_yorigin = 0.0;
  text_leading = 0.0;
  text_offset = 0.0;
}


static void text_mode (void)
{
  switch (motion_state) {
  case NO_MODE:
  case GRAPHICS_MODE:
    pdf_doc_add_to_page (" BT", 3);
    reset_text_state();
    break;
  case STRING_MODE:
    pdf_doc_add_to_page (")", 1);  /*  Fall through */
    pdf_doc_add_to_page ("]TJ", 3);
    break;
  }
  motion_state = TEXT_MODE;
}

static void graphics_mode (void)
{
  switch (motion_state) {
  case STRING_MODE:
    pdf_doc_add_to_page (")", 1); /* Fall through */
    pdf_doc_add_to_page ("]TJ", 3); /* Fall through */
  case TEXT_MODE:
    pdf_doc_add_to_page (" ET", 3);
    break;
  }
  motion_state = GRAPHICS_MODE;
}

static void string_mode (void)
{
  switch (motion_state) {
  case NO_MODE:
  case GRAPHICS_MODE:
    pdf_doc_add_to_page (" BT", 3); /* Fall through */
    reset_text_state();
    /* Following may be necessary after a rule (and also after
       specials) */
  case TEXT_MODE:
    if (ROUND(dvi_dev_xpos()-text_xorigin,0.01) == 0.0 &&
	(ROUND(dvi_dev_ypos()-text_yorigin-text_leading,0.01) == 0.0)) {
      sprintf (format_buffer, " T*[");
      text_yorigin += text_leading;
    }
    else {
      sprintf (format_buffer, " %g %g TD[",
	       ROUND(dvi_dev_xpos()-text_xorigin,0.01),
	       ROUND(dvi_dev_ypos()-text_yorigin,0.01));
      text_leading = ROUND(dvi_dev_ypos()-text_yorigin,0.01);
      text_xorigin = dvi_dev_xpos();
      text_yorigin = dvi_dev_ypos();
    }
    text_offset = 0.0;
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    pdf_doc_add_to_page ("(", 1);
    break;
  }
  motion_state = STRING_MODE;
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
  current_font = -1;
  current_phys_font = -1;
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
    sprintf (format_buffer, " q 0 w %g %g %g %g k %g %g %g %g K ",
	     background.c1, background.c2, background.c3, background.c4,
	     background.c1, background.c2, background.c3, background.c4);
    break;
  }
  pdf_doc_this_bop (format_buffer, strlen(format_buffer));
  sprintf (format_buffer,
	   " 0 0 m %g 0 l %g %g l 0 %g l b Q ",
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
  if (num_colors == 0) {
    pdf_doc_add_to_page (" 0 g 0 G", 8);
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
    pdf_doc_add_to_page (" RG", 3);
    break;
  case CMYK:
    sprintf (format_buffer, " %g %g %g %g",
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

void dev_begin_xform (double xscale, double yscale, double rotate)
{
  double c, s;
  if (num_transforms >= MAX_TRANSFORMS) {
    fprintf (stderr, "\ndev_begn_xform:  Exceeded depth of transformation stack\n");
    return;
  }
  c = ROUND (cos(rotate),1e-5);
  s = ROUND (sin(rotate),1e-5);
  sprintf (work_buffer, " q %g %g %g %g %g %g cm",
	   xscale*c, xscale*s, -yscale*s, yscale*c,
	   ROUND((1.0-xscale*c)*dvi_dev_xpos()+yscale*s*dvi_dev_ypos(),0.001),
	   ROUND(-xscale*s*dvi_dev_xpos()+(1.0-yscale*c)*dvi_dev_ypos(),0.001));
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
  /* Set page size now that we know user had last chance to change
     it */
  sprintf (format_buffer, "%g 0 0 %g 0 0 cm ", ROUND(72.0/DPI,0.001), ROUND(72.0/DPI,0.001));
  pdf_doc_this_bop (format_buffer, strlen(format_buffer));
  sprintf (format_buffer, "1 0 0 1 0 %ld cm ", (long) dev_page_height());
  pdf_doc_this_bop (format_buffer, strlen(format_buffer));
  graphics_mode();
  dev_close_all_xforms();
#ifdef MEM_DEBUG
MEM_END
#endif
}

int dev_locate_font (char *tex_name, double ptsize)
     /* Here, the ptsize is 1/72 of inch */ 
{
  /* Since Postscript fonts are scaleable, this font may have already
     been asked for.  Make sure it doesn't already exist. */
  int i, thisfont;
  if (verbose) {
    fprintf (stderr, "dev_locate_font: fontname: (%s) ptsize: %g\n",
	     tex_name, ptsize);
    fprintf (stderr, "dev_locate_font: dev_id: %d\n", n_dev_fonts);
  }
  if (n_dev_fonts == MAX_DEVICE_FONTS)
    ERROR ("dev_locate_font:  Tried to load too many fonts\n");
  /* Use an automatic variable to hold n_dev_fonts so this
     routine is *reentrant* */
  thisfont = n_dev_fonts++;
  for (i=0; i<thisfont; i++) {
    if (dev_font[i].tex_name && strcmp (tex_name, dev_font[i].tex_name) == 0) {
      break;
    }
  }
  if (i == thisfont) {  /* Font *name* not found, so load it and give it a
			    new short name */
    dev_font[thisfont].tfm_font_id = tfm_open (tex_name);
    /* type1_font_resource on next line always returns an *indirect* obj */ 
    dev_font[thisfont].font_resource = type1_font_resource (tex_name,
						     dev_font[thisfont].tfm_font_id,
						     dev_font[thisfont].short_name);
    if (dev_font[i].font_resource) { /* If we got one, it must be a physical font */
      dev_font[i].short_name[0] = 'F';
      sprintf (dev_font[i].short_name+1, "%d", ++n_phys_fonts);
      dev_font[i].type = PHYSICAL;
    } else { /* No physical font, see if we can locate a virtual one */
      dev_font[i].vf_font_id = vf_font_locate (tex_name, ptsize);
      if (dev_font[i].vf_font_id < 0) {
	fprintf (stderr, "%s: Can't locate an AFM or VF file\n", tex_name);
	ERROR ("Not sure how to proceed.  For now this is fatal\n\
Maybe in the future, I'll substitute some other font.");
      }
      dev_font[i].type = VIRTUAL;
    }
  } else {	/* Font name was found */
    /* Copy the parts that do not depend on point size */
    /* Rebuild everything else */
    dev_font[thisfont].type = dev_font[i].type;
    dev_font[thisfont].tfm_font_id = dev_font[i].tfm_font_id;
    switch (dev_font[i].type) {
    case PHYSICAL:
      strcpy (dev_font[thisfont].short_name, dev_font[i].short_name);
      dev_font[thisfont].font_resource = pdf_link_obj
	(dev_font[i].font_resource);
      break;
    case VIRTUAL:
      /* Someday this should be done right so we don't have
         to call vf_font_locate and reload the vf file for every
         point size ! We won't check the return code since
         we've located it once before we assume its still there  */
      dev_font[thisfont].vf_font_id = vf_font_locate (tex_name, ptsize);
      break;
    }
  }
  /* Note that two entries in dev_font may have the same name and
     pointsize.  This only comes into play with virtual fonts. 
     For example, if the dvi file asks for cmr10 at 12pt and
     one or more vf files also ask for cmr10 at 12pt, multiple entries
     will be generated in the font table.  Names are unique,
     so any given name will only be embedded once. The code to make
     name/ptsize combinations also unique isn't worth the
     effort for the slight increase in storage requirements. */

  dev_font[thisfont].ptsize = ptsize;
  dev_font[thisfont].tex_name = NEW (strlen(tex_name)+1, char);
  strcpy (dev_font[thisfont].tex_name, tex_name);
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

void dev_close_all_fonts(void)
{
  int i;
  for (i=0; i<n_dev_fonts; i++) {
    pdf_release_obj (dev_font[i].font_resource);
    RELEASE (dev_font[i].tex_name);
  }
  type1_close_all_encodings();
  vf_close_all_fonts();
}

void dev_select_font (int dev_font_id)
{
  if (debug) {
    fprintf (stderr, "(dev_select_font)");
  }
  if (debug) {
    fprintf (stderr, "dev_select_font: oldfont: %d newfont: %d\n",
	     current_font, dev_font_id);
  }
  if (dev_font_id < 0 || dev_font_id >= n_dev_fonts) {
    ERROR ("dev_change_to_font: dvi wants a font that isn't loaded");
  }
  current_font = dev_font_id;
  if (dev_font_id >= 0)
    current_ptsize = dev_font[dev_font_id].ptsize;
  else
    current_ptsize = 0.0;
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

void dev_need_phys_font (void)
{
  if (current_phys_font != current_font &&
      current_font >= 0 &&
      dev_font[current_font].type == PHYSICAL) {
    text_mode();
    sprintf (format_buffer, " /%s %g Tf", dev_font[current_font].short_name,
	     ROUND(dev_font[current_font].ptsize*DPI/72, 0.01));
    pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
    /* Add to Font list in Resource dictionary for this page */
    pdf_doc_add_to_page_fonts (dev_font[current_font].short_name,
			       pdf_link_obj(dev_font[current_font].font_resource));
    current_phys_font = current_font;
  }
  if (current_font < 0) {
    ERROR ("dev_need_phys_font:  Tried to put a character with no font defined\n");
  }
}

/* The following routine is here for forms.  Since
   a form is self-contained, it will need its own Tf command
   at the beginningg even if it is continuing to set type
   in the current font.  This routine simply forces reinstantiation
   of the current font. */

void dev_reselect_font(void)
{
  current_phys_font = -1;
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
  switch (dev_font[current_font].type) {
  case PHYSICAL:
    dev_need_phys_font(); /* Force a Tf since we are actually trying
			     to write a character */
    if (ROUND(dvi_dev_ypos()-text_yorigin,0.01) != 0.0 ||
	fabs(dvi_dev_xpos()-text_xorigin-text_offset) > current_ptsize)
      text_mode();
    string_mode();
    if (ROUND(dvi_dev_xpos()-text_xorigin-text_offset,0.01) != 0.0) {
      sprintf (format_buffer, ")%g(",
	       ROUND(1000.0/current_ptsize*(text_xorigin+text_offset-dvi_dev_xpos()),1.0));
      pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
      text_offset = dvi_dev_xpos()-text_xorigin;
    }
    len = pdfobj_escape_c (format_buffer, ch);
    pdf_doc_add_to_page (format_buffer, len);
    text_offset += width;
    break;
  case VIRTUAL:
    vf_set_char (ch, dev_font[current_font].vf_font_id);
  }
}

void dev_rule (double width, double height)
{
  if (debug) {
    fprintf (stderr, "(dev_rule)");
  }
  graphics_mode();
  sprintf (format_buffer, " %g %g m %g %g l %g %g l %g %g l b",
	   ROUND(dvi_dev_xpos(),0.01), ROUND(dvi_dev_ypos(),0.01),
	   ROUND(dvi_dev_xpos()+width,0.01), ROUND(dvi_dev_ypos(),0.01),
	   ROUND(dvi_dev_xpos()+width,0.01), ROUND(dvi_dev_ypos()+height,0.01),
	   ROUND(dvi_dev_xpos(),0.01), ROUND(dvi_dev_ypos()+height,0.01));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
}

/* The following routines tell the coordinates in physical PDF style
   coordinate with origin at bottom left of page.  All other
   coordinates in this routine are in TeX style coordinates */

double dev_tell_x (void)
{
  return dvi_dev_xpos();
}

double dev_tell_y (void)
{
  return dev_page_height()+dvi_dev_ypos();
}


void dev_do_special (void *buffer, UNSIGNED_QUAD size)
{
  graphics_mode();
  pdf_parse_special (buffer, size, dvi_dev_xpos(), dvi_dev_ypos());
}

