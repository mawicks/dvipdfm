/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/psimage.c,v 1.2 1999/09/01 01:00:31 mwicks Exp $

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

#include <stdio.h>
#include "system.h"
#include "mem.h"
#include "mfileio.h"
#include "pdfobj.h"
#include "psimage.h"
#include "epdf.h"

static char * distiller_template = NULL;

void set_distiller_template (char *s) 
{
  distiller_template = NEW (strlen(s)+1, char);
  strcpy (distiller_template, s);
  return;
}

#define need(n) { unsigned k=(n); \
                 if (size+k>max_size) { \
                     max_size += k+128; \
                     result=RENEW(result,max_size,char); \
                       }}
 
static char *build_command_line (char *psname, char *pdfname)
{
  char *result = NULL, *current;
  int size = 0, max_size = 0;
  if (distiller_template) {
    need(strlen(distiller_template)+1);
    for (current =distiller_template; *current != 0; current ++) {
      if (*current == '%') {
	switch (*(++current)) {
	case 'o': /* Output file name */
	  need(strlen(pdfname));
	  strcat (result, pdfname);
	  size+=strlen(pdfname);
	  break;
	case 'i': /* Input filename */
	  need(strlen(psname));
	  strcat (result, psname);
	  size+=strlen(psname);
	  break;
	case 0:
	  break;
	case '%':
	  result[size++] = '%';
	}
      } else {
	result[size++] = *current;
      }
      result[size] = 0;
    }
  } else {
    fprintf (stderr, "\nConfig file contains no template to perform PS -> PDF conversion\n");
  }
  return result;
}

pdf_obj *ps_include (char *file_name, 
		     struct xform_info *p,
		     char *res_name, double x_user, double y_user)
{
  pdf_obj *result = NULL;
  char *tmp, *cmd;
  FILE *pdf_file = NULL;
  /* Get a full qualified tmp name */
  tmp = tmpnam (NULL);
  if ((cmd = build_command_line (file_name, tmp))) {
    if (!system (cmd) && (pdf_file = FOPEN (tmp, FOPEN_RBIN_MODE))) {
      result = pdf_include_page (pdf_file, p, res_name);
    } else {
      fprintf (stderr, "\nConversion failed.\n");
    }
    if (pdf_file) {
      FCLOSE (pdf_file);
      remove (tmp);
    }
    RELEASE (cmd);
  }
  return result;
}

int check_for_ps (FILE *image_file) 
{
  rewind (image_file);
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (!strncmp (work_buffer, "%!PS", 4))
    return 1;
  return 0;
}