/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pkfont.h,v 1.3 1999/01/25 03:52:31 mwicks Exp $

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

#ifndef _PK_FONT_H
#define _PK_FONT_H

#include "pdfobj.h"

void pk_set_verbose(void);
int pk_font (char *tex_name, double ptsize, int tfm_id, char
	     *res_name);
pdf_obj *pk_font_resource (int pk_id);
char *pk_font_used (int pk_id);
void pk_close_all (void);
void pk_set_dpi (int dpi);

#endif /* _PK_FONT_H */
