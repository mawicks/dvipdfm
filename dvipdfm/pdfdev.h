/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdev.h,v 1.5 1998/12/03 22:38:11 mwicks Exp $

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

	
#ifndef PDFDEV_H
#define PDFDEV_H

#include "numbers.h"

void dev_init (char *outputfile);

void dev_close (void);

unsigned long dev_tell_xdpi(void);

unsigned long dev_tell_ydpi(void);

void dev_locate_font (char *name, unsigned long tex_font_id,
		      int tfm_font_id, double ptsize);

void dev_bop (void);

void dev_eop (void);

void dev_select_font (long tex_font_id);
void dev_reselect_font (void);

void dev_set_char (unsigned ch, double width);

void dev_set_page_size (double width, double height);

void dev_rule (double width, double height);

void dev_moveright (double length);

void dev_moveto (double x, double y);

void dev_set_bop (char *s, unsigned length);
void dev_set_eop (char *s, unsigned length);

double dev_tell_x (void);
double dev_tell_y (void);

void dev_begin_rgb_color (double r, double g, double b);
void dev_begin_cmyk_color (double c, double m, double y, double k);
void dev_begin_gray (double value);
void dev_end_color (void);
void dev_do_color(void);

void dev_bg_rgb_color (double r, double g, double b);
void dev_bg_cmyk_color (double c, double m, double y, double k);
void dev_bg_gray (double value);

void dev_begin_xform (double xscale, double yscale, double rotate);
void dev_end_xform (void);
void dev_close_all_xforms (void);

void dev_add_comment (char *comment);
void dev_do_special (void *buffer, UNSIGNED_QUAD size);

double dev_page_height(void);
double dev_page_width(void);

#endif /* PDFDEV_H */
