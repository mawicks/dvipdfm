/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/numbers.h,v 1.4 1998/12/10 17:52:17 mwicks Exp $

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

	
#ifndef NUMBERS_H
#define NUMBERS_H

#include <stdio.h>
#include <math.h>

typedef unsigned char Ubyte;

typedef int UNSIGNED_BYTE, SIGNED_BYTE, SIGNED_PAIR;
typedef unsigned UNSIGNED_PAIR;
typedef long  UNSIGNED_TRIPLE, SIGNED_TRIPLE, SIGNED_QUAD;
typedef unsigned long UNSIGNED_QUAD;

UNSIGNED_BYTE get_unsigned_byte (FILE *);

SIGNED_BYTE get_signed_byte (FILE *);

UNSIGNED_PAIR get_unsigned_pair (FILE *);

SIGNED_PAIR get_signed_pair (FILE *);

UNSIGNED_TRIPLE get_unsigned_triple (FILE *);

SIGNED_TRIPLE get_signed_triple (FILE *);

SIGNED_QUAD get_signed_quad (FILE *);

UNSIGNED_QUAD get_unsigned_quad (FILE *);

#define ROUND(n,acc) (floor(((double)n)/(acc)+0.5)*(acc)) 

SIGNED_QUAD sqxfw (SIGNED_QUAD sq, SIGNED_QUAD fw);

#endif /* NUMBERS_H */

