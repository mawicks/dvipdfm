/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/tpic.c,v 1.1 1999/01/20 06:09:47 mwicks Exp $

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

#include <string.h>
#include <stdlib.h>
#include "tpic.h"
#include "pdfparse.h"
#include "mem.h"
#include "mfileio.h"
#include "pdfdoc.h"

#define MI2PT (0.072)

double pen_size = 14.0;
int fill_shape = 0;
double fill_color = 0.0;
struct path
{
  double x, y;
} *path = NULL;
unsigned long path_length = 0, max_path_length = 0;

void tpic_clear_state (void) 
{
  pen_size = 14.0;
  if ((path))
    RELEASE(path);
  path_length = 0;
  max_path_length = 0;
  fill_shape = 0;
  fill_color = 0.0;
  return;
}

static void set_pen_size (char **buffer, char *end)
{
  char *number;
  skip_white (buffer, end);
  if ((number = parse_number(buffer, end))) {
    pen_size = atof (number) * MI2PT;
    RELEASE (number);
  } else {
    dump (*buffer, end);
    fprintf (stderr, "tpic special: pn: Invalid pen size\n");
  }
}

static void set_fill_color (char **buffer, char *end)
{
  char *number;
MEM_START
  fprintf (stderr, "set_fill_color\n");
  skip_white (buffer, end);
  if ((number = parse_number(buffer, end))) {
    fill_color = 1.0 - atof (number);
    if (fill_color > 1.0)
      fill_color = 1.0;
    if (fill_color < 0.0)
      fill_color = 0.0;
    RELEASE (number);
    fill_shape = 1;
  } else {
    dump (*buffer, end);
    fprintf (stderr, "tpic special: sh: Invalid shade\n");
  }
MEM_END
}

static void add_point (char **buffer, char *end) 
{
  char *x= NULL, *y= NULL;
MEM_START
  skip_white (buffer, end);
  if (*buffer < end)
    x = parse_number (buffer, end);
  skip_white (buffer, end);
  if (*buffer < end)
    y = parse_number (buffer, end);
  if ((x) && (y)) {
    if (path_length >= max_path_length) {
      max_path_length += 256;
      path = RENEW (path, max_path_length, struct path);
    }
    path[path_length].x = atof(x)*MI2PT;
    path[path_length].y = atof(y)*MI2PT;
    fprintf (stderr, "added x=%g, y=%g\n", path[path_length].x,
	     path[path_length].y);
    path_length += 1;
  } else {
    dump (*buffer, end);
    fprintf (stderr, "tpic special: pa: Missing coordinate\n");
  }
  if (x) RELEASE(x);
  if (y) RELEASE(y);
MEM_END
  return;
}

static void flush_path (double x_user, double y_user, int hidden)
{
  int len;
MEM_START
  if (path_length > 1) {
    int i;
    len = sprintf (work_buffer, " q 1.4 M %.2f w", pen_size);
    pdf_doc_add_to_page (work_buffer, len);
    if (fill_shape) {
      len = sprintf (work_buffer, " %.2f g", fill_color);
      pdf_doc_add_to_page (work_buffer, len);
    }
    len = sprintf (work_buffer, " %.2f %.2f m",
		   x_user+path[0].x, y_user-path[0].y);
    pdf_doc_add_to_page (work_buffer, len);
    for (i=0; i<path_length; i++) {
      len = sprintf (work_buffer, " %.2f %.2f l", x_user+path[i].x, y_user-path[i].y);
      pdf_doc_add_to_page (work_buffer, len);
    } 
    {
      RELEASE (path);
      path = NULL;
      path_length = 0;
      max_path_length = 0;
    }
    if (!hidden && fill_shape) {
      pdf_doc_add_to_page (" b", 2);
    }
    if (hidden && fill_shape) {
      pdf_doc_add_to_page (" f", 2);
    }
    if (!hidden && !fill_shape)
      pdf_doc_add_to_page (" S", 2);
    if (fill_shape)
      fill_shape = 0;
    pdf_doc_add_to_page (" Q", 2);
  } else {
    fprintf (stderr, "tpic special: fp: Not enough points!\n");
  }
MEM_END
  return;
}


#define TPIC_PN 1
#define TPIC_PA 2
#define TPIC_FP 3
#define TPIC_IP 4
#define TPIC_DA 5
#define TPIC_DT 6
#define TPIC_SP 7
#define TPIC_AR 8
#define TPIC_IA 9
#define TPIC_SH 10
#define TPIC_WH 11
#define TPIC_BK 12
#define TPIC_TX 13

struct {
  char *s;
  int tpic_command;
} tpic_specials[] = {
  {"pn", TPIC_PN},
  {"pa", TPIC_PA},
  {"fp", TPIC_FP},
  {"ip", TPIC_IP},
  {"da", TPIC_DA},
  {"dt", TPIC_DT},
  {"sp", TPIC_SP},
  {"ar", TPIC_AR},
  {"ia", TPIC_IA},
  {"sh", TPIC_SH},
  {"wh", TPIC_WH},
  {"bk", TPIC_BK},
  {"tx", TPIC_TX}
};

int tpic_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user)
{
  int i, tpic_command, result = 0;
  char *end = buffer + size;
  skip_white (&buffer, end);
  for (i=0; i<(sizeof(tpic_specials)/sizeof(tpic_specials[0])); i++) {
    if (!strncmp (tpic_specials[i].s, buffer, strlen(tpic_specials[i].s)))
      break;
  }
  if (i < sizeof(tpic_specials)/sizeof(tpic_specials[0])) {
    tpic_command = tpic_specials[i].tpic_command;
    buffer += strlen (tpic_specials[i].s);
    result = 1;
    switch (tpic_command) {
    case TPIC_PN:
      set_pen_size (&buffer, end);
      break;
    case TPIC_PA:
      add_point (&buffer, end);
      break;
    case TPIC_FP:
      flush_path(x_user, y_user, 0);
      break;
    case TPIC_IP:
      flush_path(x_user, y_user, 1);
      break;
    case TPIC_DA:
      flush_path(x_user, y_user, 0);
      break;
    case TPIC_DT:
      flush_path(x_user, y_user, 0);
      break;
    case TPIC_SP:
      flush_path(x_user, y_user, 0);
      break;
    case TPIC_AR:
      break;
    case TPIC_IA:
      break;
    case TPIC_SH:
      set_fill_color (&buffer, end);
      break;
    case TPIC_WH: 
      fill_shape = 1;
      fill_color = 1;
      break;
    case TPIC_BK:
      fill_shape = 1;
      fill_color = 0;
      break;
    case TPIC_TX:
      fill_shape = 1;
      fill_color = 0.5;
      break;
    default:
      fprintf (stderr, "Fix me, I'm broke.  This should never happen");
      exit(1);
    }
  } else {
    result = 0;
  }
  return result;
}



