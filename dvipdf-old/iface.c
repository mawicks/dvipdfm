/* emtpy comment */
#include "iface.h"
#include "pdfdoc.h"
#include "string.h"
#include "pdfobj.h"
#include "error.h"
#include "numbers.h"
#include "type1.h"
#include "mem.h"

void dev_init (char *outputfile)
{
  pdf_doc_init (outputfile);
}

#define DPI 7200
#define PAGE_WIDTH 61200l
#define PAGE_HEIGHT 79200l

unsigned dev_tell_xdpi(void)
{
  return DPI;
}

unsigned dev_tell_ydpi(void)
{
  return DPI;
}

#define MAX_DEVICE_FONTS 256

struct dev_font {
  char short_name[7];	/* Needs to be big enough to hold name "Fxxx"
			   where xxx is number of largest font */
  char *tex_name;
  long tex_font_id;
  double ptsize;
  pdf_obj *font_resource;
} dev_font[MAX_DEVICE_FONTS];

int n_dev_fonts = 0;
int current_font = -1;
double current_ptsize = 1.0;

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
    dev_font[i].font_resource = type1_font_resource (tex_name,
						     dev_font[i].short_name);
  } else {	/* Font was found */
    strcpy (dev_font[n_dev_fonts].short_name, dev_font[i].short_name);
    dev_font[n_dev_fonts].font_resource = pdf_ref_obj (dev_font[i].font_resource);
  }
  dev_font[n_dev_fonts].tex_font_id = tex_font_id;
  dev_font[n_dev_fonts].ptsize = ptsize;
  dev_font[n_dev_fonts].tex_name = NEW (strlen(tex_name)+1, char);
  strcpy (dev_font[n_dev_fonts].tex_name, tex_name);
  n_dev_fonts += 1;
}

int motion_state = 0;  /* Start out in undefined mode */

#define GRAPHICS_MODE 1
#define TEXT_MODE 2
#define STRING_MODE 3
#define LINE_MODE 4

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
    pdf_doc_add_to_page (")", 1);
  case LINE_MODE:
    pdf_doc_add_to_page ("]TJ ", 4);
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
   pdf_doc_add_to_page (" BT ", 4);
 case TEXT_MODE:
   pdf_doc_add_to_page ("[(", 2);
   break;
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
   pdf_doc_add_to_page (" BT ", 4);
 case TEXT_MODE:
   pdf_doc_add_to_page ("[", 1);
   break;
 case STRING_MODE:
   pdf_doc_add_to_page (")", 1);
   break;
 }
 motion_state = LINE_MODE;
}

  
static char format_buffer[256];

void dev_change_to_font (long tex_font_id)
{
  int i;
  for (i=0; i<n_dev_fonts; i++) {
    if (dev_font[i].tex_font_id == tex_font_id)
      break;
  }
  if (i == n_dev_fonts) {
    ERROR ("dev_change_to_font:  dvi wants a font that isn't loaded");
  }
  assert_text_mode();
  sprintf (format_buffer, " /%s %f Tf ", dev_font[i].short_name,
	   ROUND(dev_font[i].ptsize,0.01));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  current_font = i;
  current_ptsize = dev_font[i].ptsize;
}

static pdf_obj *this_page_fontlist_dict;

void dev_bop (void)
{
  pdf_doc_new_page (PAGE_WIDTH, PAGE_HEIGHT);
#define BOP_STRING ""
  pdf_doc_add_to_page (BOP_STRING, strlen(BOP_STRING));
  assert_graphics_mode();
  sprintf (format_buffer, "0.01 0 0 0.01 0 0 cm ");
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  sprintf (format_buffer, "1 0 0 1 0 %ld cm ", (long) PAGE_HEIGHT);
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
  this_page_fontlist_dict = pdf_new_dict ();
}

void dev_eop (void)
{
  assert_graphics_mode();
#define EOP_STRING ""
  pdf_doc_add_to_page (EOP_STRING, strlen(EOP_STRING));
  pdf_doc_add_to_page_resources ("Font", this_page_fontlist_dict);
  pdf_release_object (this_page_fontlist_dict);
}

void dev_set_char (unsigned ch)
{
  char c;
  c = ch;
  assert_string_mode();
  pdf_doc_add_to_page (&c, 1);
}

void dev_rule (long x, long y, long width, long height)
{
  assert_graphics_mode();
  sprintf (format_buffer, "%ld %ld m %ld %ld l %ld %ld l %ld %ld b ",
	   x, -y, x+width, -y, x+width, -(y+height), x, -(y+height));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
}

void dev_right (long x)
{
  assert_line_mode();
  sprintf (format_buffer, "%ld", (long) -ROUND(0.01*x/current_ptsize,1.0));
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
}

void dev_gotoxy (long x, long y)
{
  assert_text_mode();
  sprintf (format_buffer, "1 0 0 1 %ld %ld Td ",
	   x, -y);
  pdf_doc_add_to_page (format_buffer, strlen(format_buffer));
}

