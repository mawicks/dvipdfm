/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/colorsp.c,v 1.3 1999/09/05 21:01:20 mwicks Exp $
    
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
#include "pdfdev.h"
#include "pdfparse.h"
#include "dvipdfm.h"

static void do_color_special (char **start, char *end)
{
  char *command, *token;
  char *c1=NULL, *c2=NULL, *c3=NULL, *c4=NULL;
  skip_white (start, end);
  if ((command = parse_ident (start, end))) {
    if (!strcmp (command, "push")) { /* Handle a push */ 
      skip_white (start, end);
      if ((token = parse_ident (start, end))) {
	if (!strcmp (token, "rgb")) { /* Handle rgb color */
	  if ((c1=parse_number(start, end)) &&
	      (c2=parse_number(start, end)) &&
	      (c3=parse_number(start, end))) {
	    dev_begin_rgb_color (atof(c1), atof(c2), atof(c3));
	  }
	}
	else if (!strcmp (token, "cmyk")) { /* Handle rgb color */
	  if ((c1=parse_number(start, end)) &&
	      (c2=parse_number(start, end)) &&
	      (c3=parse_number(start, end)) &&
	      (c4=parse_number(start, end))) {
	    dev_begin_cmyk_color (atof(c1), atof(c2), atof(c3), atof(c4));
	  }
	} else if (!strcmp (token, "gray")) { /* Handle gray */
	  if ((c1=parse_number(start, end))) {
	    dev_begin_gray (atof(c1));
	  }
	} else if (!strcmp (token, "hsb")) {
	  fprintf (stderr, "\ncolor special: hsb not implemented\n");
	} else { /* Must be a "named" color */
	  dev_begin_named_color (token);
	}
	if (c1) RELEASE(c1);
	if (c2) RELEASE(c2);
	if (c3) RELEASE(c3);
	if (c4) RELEASE(c4);
	RELEASE (token);
      }
    } else if (!strcmp (command, "pop")) { /* Handle a pop */
      dev_end_color();
    } else { /* Assume this is a default color change */
      token = command;
      if (!strcmp (token, "rgb")) { /* Handle rgb color */
	if ((c1=parse_number(start, end)) &&
	    (c2=parse_number(start, end)) &&
	    (c3=parse_number(start, end))) {
	  dev_set_def_rgb_color (atof(c1), atof(c2), atof(c3));
	}
      }
      else if (!strcmp (token, "cmyk")) { /* Handle rgb color */
	if ((c1=parse_number(start, end)) &&
	    (c2=parse_number(start, end)) &&
	    (c3=parse_number(start, end)) &&
	    (c4=parse_number(start, end))) {
	  dev_set_def_cmyk_color (atof(c1), atof(c2), atof(c3), atof(c4));
	}
      } else if (!strcmp (token, "gray")) { /* Handle gray */
	if ((c1=parse_number(start, end))) {
	  dev_set_def_gray (atof(c1));
	}
      } else if (!strcmp (token, "hsb")) {
	fprintf (stderr, "\ncolor special: hsb not implemented\n");
      } else { /* Must be a "named" color */
	dev_set_def_named_color (token);
      }
      if (c1) RELEASE(c1);
      if (c2) RELEASE(c2);
      if (c3) RELEASE(c3);
      if (c4) RELEASE(c4);
    }
  }
  RELEASE (command);
}

static void do_background_special (char **start, char *end)
{
  char *token;
  char *c1=NULL, *c2=NULL, *c3=NULL, *c4=NULL;
  skip_white (start, end);
  if ((token = parse_ident (start, end))) {
    if (!strcmp (token, "rgb")) { /* Handle rgb color */
      if ((c1=parse_number(start, end)) &&
	  (c2=parse_number(start, end)) &&
	  (c3=parse_number(start, end))) {
	dev_bg_rgb_color (atof(c1), atof(c2), atof(c3));
      }
    }
    else if (!strcmp (token, "cmyk")) { /* Handle rgb color */
      if ((c1=parse_number(start, end)) &&
	  (c2=parse_number(start, end)) &&
	  (c3=parse_number(start, end)) &&
	  (c4=parse_number(start, end))) {
	dev_bg_cmyk_color (atof(c1), atof(c2), atof(c3), atof(c4));
      }
    } else if (!strcmp (token, "gray")) { /* Handle gray */
      if ((c1=parse_number(start, end))) {
	dev_bg_gray (atof(c1));
      }
    } else if (!strcmp (token, "hsb")) {
      fprintf (stderr, "\ncolor special: hsb not implemented\n");
    } else { /* Must be a "named" color */
      dev_bg_named_color (token);
    }
    if (c1) RELEASE(c1);
    if (c2) RELEASE(c2);
    if (c3) RELEASE(c3);
    if (c4) RELEASE(c4);
    RELEASE (token);
  }
}

int color_special (char *buffer, UNSIGNED_QUAD size)
{
  char *start = buffer, *end;
  int result = 0;
  end = buffer + size;
  skip_white (&start, end);
  if (!strncmp (start, "color", strlen("color"))) {
    start += strlen("color");
    result = 1; /* This is a color special */
    do_color_special (&start, end);
  } else if (!strncmp (start, "background", strlen("background"))) {
    start += strlen("background");
    result = 1; /* This is a color special */
    do_background_special (&start, end);
  } else if (!strncmp (start, "landscape", strlen("landscape"))) {
    set_landscape_mode();
    result = 1;
  }
  return result;
}






