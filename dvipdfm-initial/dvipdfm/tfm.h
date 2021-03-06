/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/tfm.h,v 1.2 1998/11/18 02:31:35 mwicks Exp $

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

	
#ifndef TFM_H

#define TFM_H

#include "numbers.h"

void tfm_set_verbose(void);
void tfm_set_debug(void);

int tfm_open(char * tex_font_name);

double tfm_get_width (int font_id, UNSIGNED_PAIR ch);
UNSIGNED_PAIR tfm_get_firstchar (int font_id);
UNSIGNED_PAIR tfm_get_lastchar (int font_id);

#endif /* TFM_H */
