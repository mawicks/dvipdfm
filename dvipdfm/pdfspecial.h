/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfspecial.h,v 1.16 1999/09/22 02:26:17 mwicks Exp $

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
  unsigned char user_bbox;
  unsigned char clip;
  double llx, lly, urx, ury;
};

extern int pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user);
extern void pdf_finish_specials(void);
extern pdf_obj *get_reference(char **start, char *end);
extern struct xform_info *new_xform_info (void);
extern void release_xform_info (struct xform_info *p);
extern int validate_image_xform_info (struct xform_info *p);
extern void add_xform_matrix (double xoff, double yoff, double xscale, double
		       yscale, double rotate);

extern void pdf_special_ignore_colors(void);

extern double parse_one_unit (char **start, char *end);
extern void pdf_scale_image (struct xform_info *p, double nat_width,
			     double nat_height);
extern pdf_obj *embed_image (char *filename, struct xform_info *p, 
			     double x_user, double y_user, char
			     *objname);
extern void pdf_finish_specials(void);
extern void pdf_special_set_grow (double g);
extern double pdf_special_tell_grow (void);

#ifndef M_PI
  #define M_PI (4.0*atan(1.0))
#endif /* M_PI */

#endif  /* PDFSPECIAL_H */
