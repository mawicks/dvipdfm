/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/dvi.h,v 1.4 1998/12/09 04:04:30 mwicks Exp $

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

	

#include "error.h"
#include "numbers.h"

error_t dvi_open (char *filename);

void dvi_set_verbose (void);
void dvi_set_debug (void);

void dvi_close (void);  /* Closes data structures created by dvi_open */
void dvi_complete (void);  /* Closes output file being written by an
			      actual driver */

void dvi_init (char *outputfile);

double dvi_tell_mag (void);
double dvi_unit_size (void);

void dvi_vf_init (int dev_font_id);
void dvi_vf_finish (void);

void dvi_set (SIGNED_QUAD ch);
void dvi_rule (SIGNED_QUAD width, SIGNED_QUAD height);
void dvi_right (SIGNED_QUAD x);
void dvi_put (SIGNED_QUAD ch);
void dvi_push (void);
void dvi_pop (void);
void dvi_w0 (void);
void dvi_w (SIGNED_QUAD ch);
void dvi_x0(void);
void dvi_x (SIGNED_QUAD ch);
void dvi_down (SIGNED_QUAD y);
void dvi_y (SIGNED_QUAD ch);
void dvi_y0(void);
void dvi_z (SIGNED_QUAD ch);
void dvi_z0(void);
