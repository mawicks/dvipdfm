/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/mpost.c,v 1.2 1999/08/24 03:38:04 mwicks Exp $

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

#include "mfileio.h"
#include "pdfobj.h"
#include "pdfspecial.h"
#include "mpost.h"
#include "string.h"

int check_for_mp (FILE *image_file) 
{
  rewind (image_file);
  /* For now, this is an exact test that must be passed, character for
     character */
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strncmp (work_buffer, "%!PS", 4))
    return 0;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strlen(work_buffer)>=strlen("%%BoundingBox") &&
      strncmp (work_buffer, "%%BoundingBox", strlen("%%BoundingBox")))
    return 0;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strlen(work_buffer) >= strlen("%%Creator: MetaPost") &&
      strncmp (work_buffer, "%%Creator: MetaPost", strlen("%%Creator: MetaPost")))
    return 0;
  fprintf (stderr, "\nMetapost file\n");
  return 1;
}

pdf_obj *mp_include (FILE *image_file,  struct xform_info *p,
		     char *res_name) 
{
  return NULL;
}



