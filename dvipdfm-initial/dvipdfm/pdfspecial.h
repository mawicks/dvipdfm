/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/pdfspecial.h,v 1.5 1998/11/22 21:37:58 mwicks Exp $

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

	
#ifndef PDFSPECIAL_H

#define PDFSPECIAL_H
#include "numbers.h"
#include "pdfobj.h"

struct xform_info 
{
  double width;
  double height;
  double depth;
  double scale;
  double xscale;
  double yscale;
  double rotate;
};

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user, double x_media, double
		       y_media);
void pdf_finish_specials(void);
pdf_obj *get_reference(char **start, char *end);

void add_xform_matrix (double xoff, double yoff, double xscale, double
		       yscale, double rotate);

#endif  /* PDFSPECIAL_H */
