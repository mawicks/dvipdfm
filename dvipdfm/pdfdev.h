/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdev.h,v 1.25 1999/08/26 04:51:03 mwicks Exp $

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

	
#ifndef PDFDEV_H
#define PDFDEV_H

#include "numbers.h"

typedef signed long mpt_t;

extern void dev_init(double scale, double x_offset, double y_offset);

extern void dev_close (void);

extern unsigned long dev_tell_xdpi(void);

extern unsigned long dev_tell_ydpi(void);

extern int dev_locate_font (char *name, mpt_t ptsize);
extern int dev_font_tfm (int dev_font_id);
extern double dev_font_size (int dev_font_id);
extern mpt_t dev_font_mptsize (int dev_font_id);
extern void dev_close_all_fonts (void);

extern void dev_bop (void);

extern void dev_eop (void);

extern void dev_select_font (int tex_font_id);
extern void dev_reselect_font (void);
extern void dev_set_char (mpt_t xpos, mpt_t ypos, unsigned char ch,
			  mpt_t width, int font_id);
extern void dev_set_string (mpt_t xpos, mpt_t ypos, unsigned char *ch,
			    int len, mpt_t width, int font_id);

extern void dev_set_page_size (double width, double height);

extern void dev_rule (mpt_t xpos, mpt_t ypos, mpt_t width, mpt_t height);

extern void dev_set_bop (char *s, unsigned length);
extern void dev_set_eop (char *s, unsigned length);

extern double dev_phys_x (void);
extern double dev_phys_y (void);

extern void dev_begin_rgb_color (double r, double g, double b);
extern void dev_begin_cmyk_color (double c, double m, double y, double k);
extern void dev_begin_gray (double value);
extern void dev_end_color (void);
extern void dev_do_color(void);

extern void dev_bg_rgb_color (double r, double g, double b);
extern void dev_bg_cmyk_color (double c, double m, double y, double k);
extern void dev_bg_gray (double value);

extern void dev_begin_xform (double xscale, double yscale, double
			     rotate, double x_user, double y_user);
extern void dev_end_xform (void);
extern void dev_close_all_xforms (void);

extern void dev_add_comment (char *comment);
extern void dev_do_special (void *buffer, UNSIGNED_QUAD size, double
			    x_user, double y_user);

extern double dev_page_height(void);
extern double dev_page_width(void);
/* These last two are hacks to get metapost inclusion to work */
extern double dev_dvi2pts (void);
extern void graphics_mode (void);

#endif /* PDFDEV_H */
