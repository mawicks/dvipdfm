#include "pdfdev.h"
#include "pdfdoc.h"
#include "string.h"
#include "pdfobj.h"
#include "error.h"
#include "numbers.h"
#include "type1.h"
#include "mem.h"

#define DPI 72u  /* Acrobat doesn't seem to line coordinate systems
		    that involve scalings around 0.01 */ 
/* Following dimensions in virtual device coordinates,
which are 0.01 points */

#define PAGE_WIDTH (17u*DPI/2u)
#define PAGE_HEIGHT (11U*DPI)

unsigned dev_tell_xdpi(void)
{
  return DPI;
}

unsigned dev_tell_ydpi(void)
{
  return DPI;
}

int motion_state = 1; /* Start in graphics mode */

#define GRAPHICS_MODE 1
#define TEXT_MODE 2
#define STRING_MODE 3
#define LINE_MODE 4

static char format_buffer[256];
 /* Device coordinates are relative to upper left of page.  One of the
    first things appearing on the page is a coordinate transformation
    matrix that forces this to be true.  This coordinate
    transformation is the only place where the paper size is required.
    Unfortunately, positive is up, which doesn't agree with TeX's convention.  */

static double dev_xpos, dev_ypos; 

void assert_text_mode (void)
{
  switch (motion_state) {
  case GRAPHICS_MODE:
    pdf_doc_add_to_page (" BT ", 4);
    break;
  case STRING_MODE:
    pdf_doc_add_to_page (")", 1);  /*  Fall through */
  case LINE_MODE:
    pdf_doc_add_to_page ("]TJ ", 4);
    break;
  }
  motion_state = TEXT_MODE;
}

void assert_graphics_mode (void)
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

void assert_string_mode (void)
{
 switch (motion_state) {
 case GRAPHICS_MODE:
   pdf_doc_add_to_page (" BT ", 4); /* Fall through */
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

void assert_line_mode (void)
{
 switch (motion_state) {
 case GRAPHICS_MODE:
   pdf_doc_add_to_page (" BT ", 4); /* Fall through */
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
  pdf_doc_init (outputfile);
  assert_graphics_mode();
}

void dev_close (void)
{
  assert_graphics_mode();
  pdf_doc_finish ();
}


/*  BOP, EOP, and FONT section.
   BOP and EOP manipulate some of the same data structures
   as the font stuff */ 

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

void bop_font_reset(void)
{
  int i;
  for (i=0; i<n_dev_fonts; i++) {
    dev_font[i].used_on_this_page = 0;
  }
}

void dev_bop (void)
{
  pdf_doc_new_page (PAGE_WIDTH*72u/DPI, PAGE_HEIGHT*72u/DPI);
#define BOP_STRING ""
  pdf_doc_add_to_page (BOP_STRING, strlen(BOP_STRING));
  assert_graphics_mode();
  dev_xpos = 0.0;
  dev_ypos = 0.0;
  sprintf (format_buffer, "%g 0 0 %g 0 0 cm ", ROUND(72.0/DPI,0.001), ROUND(72.0/DPI,0.001));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  sprintf (format_buffer, "1 0 0 1 0 %ld cm ", (long) PAGE_HEIGHT);
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  this_page_fontlist_dict = pdf_new_dict ();
  bop_font_reset();
}

void dev_eop (void)
{
  assert_graphics_mode();
#define EOP_STRING ""
  pdf_doc_add_to_page (EOP_STRING, strlen(EOP_STRING));
  pdf_doc_add_to_page_resources ("Font", this_page_fontlist_dict);
  pdf_release_obj (this_page_fontlist_dict);
}

void dev_locate_font (char *tex_name,
		      unsigned long tex_font_id, double ptsize)
{
  /* Since Postscript fonts are scaleable, this font may have already
     been asked for.  Make sure it doesn't already exist. */
  int i;
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
  pdf_obj *tmp1, tmp2;
  for (i=0; i<n_dev_fonts; i++) {
    if (dev_font[i].tex_font_id == tex_font_id)
      break;
  }
  if (i == n_dev_fonts) {
    ERROR ("dev_change_to_font:  dvi wants a font that isn't loaded");
  }
  assert_text_mode();
  sprintf (format_buffer, " /%s %g Tf ", dev_font[i].short_name,
	   ROUND(dev_font[i].ptsize*DPI/72,0.01));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  current_font = i;
  current_ptsize = dev_font[i].ptsize;
  /* Add to resource list for this page */
  if (!dev_font[i].used_on_this_page) {
    pdf_add_dict (this_page_fontlist_dict,
		  tmp1 = pdf_new_name (dev_font[i].short_name),
		  dev_font[i].font_resource);
    pdf_release_obj (tmp1);
    dev_font[i].used_on_this_page = 1;
  }
}


void dev_set_char (unsigned ch)
{
  char c;
  int len;
  c = ch;
  assert_string_mode();
  len = pdfobj_escape_c (format_buffer, c);
  pdf_doc_add_to_page (format_buffer, len);
}

void dev_rule (double width, double height)
{
  assert_graphics_mode();
  sprintf (format_buffer, "%g %g m %g %g l %g %g l %g %g l b ",
	   ROUND(dev_xpos,0.01), ROUND(dev_ypos,0.01),
	   ROUND(dev_xpos+width,0.01), ROUND(dev_ypos,0.01),
	   ROUND(dev_xpos+width,0.01), ROUND(dev_ypos+height,0.01),
	   ROUND(dev_xpos,0.01), ROUND(dev_ypos+height,0.01));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
}

void dev_moveright (double x)
{

  assert_line_mode();
  sprintf (format_buffer, "%g", -ROUND(72000.0/current_ptsize*x/DPI,0.1));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
}

void dev_moveto (double x, double y)
{
  dev_xpos = x;
  dev_ypos = -y;
  assert_text_mode();
}

