/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/htex.c,v 1.4 1999/09/05 02:56:36 mwicks Exp $

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

#include <string.h>
#include <stdlib.h>
#include "htex.h"
#include "pdfparse.h"
#include "mem.h"
#include "mfileio.h"
#include "pdfdoc.h"
#include "dvi.h"
#include "ctype.h"

static int is_htex_special (char **start, char *end)
{
  skip_white(start, end);
  if (end-*start >= strlen ("html:") &&
      !strncmp (*start, "html:", strlen("html:"))) {
    *start += strlen("html:");
    return 1;
  }
  return 0;
}

static void downcase (char *s)
{
  while (*s != 0) {
    if (*s >= 'A' && *s <= 'Z')
      *s = (*s-'A')+'a';
    s += 1;
  }
}


#define ANCHOR 0
#define IMAGE 1
#define BASE 2
#define END_ANCHOR 3

static char *tags[] = {"a", "img", "base" };

static int parse_html_tag (char **start, char *end)
{
  int result = -1;
  char *token = NULL;
  int closing = 0;
  skip_white(start, end);
  if (*start < end) {
    if (**start == '/') {
      (*start)++;
      closing = 1;
    }
    if (*start < end && (token = parse_ident (start, end))) {
      downcase (token);
      {
	int i;
	for (i=0; i<sizeof(tags)/sizeof(tags[0]); i++) {
	  if (!strcmp (token, tags[i])) {
	    result = i;
	    if (closing) 
	      result += sizeof(tags)/sizeof(tags[0]);
	  }
	  break;
	}
	if (i>=sizeof(tags)/sizeof(tags[0]))
	  result = -1;
      }
      RELEASE (token);
    }
  }
  return result;
}

static pending = 0;
char *pending_value;
double pending_x, pending_y;
int pending_type;

#define HREF 1
#define NAME 2

void html_start_anchor (char *key, char *value) 
{
  if (pending) {
    fprintf (stderr, "\nWarning: Nexted html anchors\n");
  }
  if (!strcmp (key, "href") || !strcmp(key, "name")) {
    pending+=1;
    pending_x = dev_phys_x();
    pending_y = dev_phys_y();
    pending_value = value;
    if (!strcmp (key, "href")) {
      pending_type = HREF;
    } else if (!strcmp (key, "name")) {
      pending_type = NAME;
    }
  } else {
    pending_value = NULL;
    if (value)
      RELEASE (value);
  }
  if (key)
    RELEASE (key);
}

void html_make_dest (char *name) 
{
  pdf_obj *array;
  array = pdf_new_array ();
  pdf_add_array (array, pdf_doc_this_page_ref());
  pdf_add_array (array, pdf_new_name("XYZ"));
  pdf_add_array (array, pdf_new_null());
  pdf_add_array (array, pdf_new_number(dev_phys_y()+24.0));
  pdf_add_array (array, pdf_new_null());
  pdf_doc_add_dest (name, strlen(name), pdf_ref_obj(array));
  pdf_release_obj (array);
}

char *base_value = NULL;

void html_make_link (char *name, double xll, double yll, double xur, double yur) 
{
  pdf_obj *link, *box, *color;
  link = pdf_new_dict();
  pdf_add_dict(link, pdf_new_name("Type"), pdf_new_name ("Annot"));
  pdf_add_dict(link, pdf_new_name("Subtype"), pdf_new_name ("Link"));
  box = pdf_new_array ();
  pdf_add_array (box, pdf_new_number (xll-2.0));
  pdf_add_array (box, pdf_new_number (yll-2.0));
  pdf_add_array (box, pdf_new_number (xur+2.0));
  /* Set a minimum box height for macro packages or authors
     that aren't very smart */
  pdf_add_array (box, pdf_new_number (abs(yur-yll)>11.0?yur+2.0:yur+13.0));
  pdf_add_dict(link, pdf_new_name("Rect"), box);
  color = pdf_new_array ();
  pdf_add_array (color, pdf_new_number (0));
  pdf_add_array (color, pdf_new_number (1));
  pdf_add_array (color, pdf_new_number (1));
  pdf_add_dict(link, pdf_new_name("C"), color);
  if (name && *name == '#' && !(base_value)) {
    pdf_add_dict (link, pdf_new_name("Dest"), pdf_new_string(name+1,strlen(name+1)));
  } else if (name) {    /* Assume its a URL */
    char *url;
    int len;
    pdf_obj *action;
    len = strlen(name)+1;
    if (base_value)
      len+=strlen(base_value);
    url = NEW (len, char);
    if (base_value)
      strcpy (url, base_value);
    else
      url[0] = 0;
    strcat (url, name);
    action = pdf_new_dict();
    pdf_add_dict (action, pdf_new_name ("Type"), pdf_new_name ("Action"));
    pdf_add_dict (action, pdf_new_name ("S"), pdf_new_name ("URI"));
    pdf_add_dict (action, pdf_new_name ("URI"),
		  pdf_new_string (url, len));
    pdf_add_dict (link, pdf_new_name ("A"), pdf_ref_obj (action));
    pdf_release_obj (action);
    RELEASE (url);
  }
  pdf_doc_add_to_page_annots (pdf_ref_obj (link));
  pdf_release_obj (link);
}

void html_end_anchor (void)
{
  if (!pending) {
    fprintf (stderr, "\nhtml_end_anchor:  Ending anchor tag without starting tag!\n");
  }
  if (pending > 0) {
    pending--;
  }
  switch (pending_type) {
  case NAME:
    html_make_dest (pending_value);
    break;
  case HREF:
    html_make_link (pending_value, pending_x, pending_y, dev_phys_x(),
		    dev_phys_y());
    break;
  default:
    fprintf (stderr, "html_end_anchor:  Uh Oh!  This can't happen\n");
    exit(1);
  }
  if (pending_value)
    RELEASE (pending_value);
}

void html_set_base (char *value)
{
  if (base_value)
    RELEASE (base_value);
  base_value = value;
}



int htex_parse_special(char *buffer, UNSIGNED_QUAD size)
{
  int result = 0;
  char *key, *value;
  char *save = buffer;
  char *end = buffer + size;
  int htmltag;
  skip_white (&buffer, end);
  if (is_htex_special(&buffer, end)) {
    result = 1; /* Must be html special (doesn't mean it will succeed) */
    skip_white (&buffer, end);
    if (buffer < end && *(buffer++) == '<' ) {
      htmltag = parse_html_tag(&buffer, end);
      switch (htmltag) {
      case ANCHOR:
	parse_key_val (&buffer, end, &key, &value);
	if (key && value)
	  html_start_anchor (key, value);
	break;
      case IMAGE:
	fprintf (stderr, "\nImage html tag not yet implemented\n");
	parse_key_val (&buffer, end, &key, &value);
	RELEASE (key);
	RELEASE (value);
	break;
      case BASE:
	parse_key_val (&buffer, end, &key, &value);
	if (key && value)
	  html_set_base (value);
	RELEASE (key);
	break;
      case END_ANCHOR:
	html_end_anchor ();
	break;
      default:
	fprintf (stderr, "Invalid tag\n");
	dump (save, end);
      }
    }
    skip_white(&buffer, end);
    if (buffer >= end || *buffer != '>') {
      fprintf (stderr, "\nBadly terminated tag..\n");
    }
  }
  return result;
}



