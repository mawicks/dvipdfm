/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/numbers.h,v 1.9 1999/02/21 14:30:21 mwicks Exp $

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

	
#ifndef NUMBERS_H
#define NUMBERS_H

#include <stdio.h>
#include <math.h>

typedef unsigned char Ubyte;

typedef int UNSIGNED_BYTE, SIGNED_BYTE, SIGNED_PAIR;
typedef unsigned UNSIGNED_PAIR;
typedef long  UNSIGNED_TRIPLE, SIGNED_TRIPLE, SIGNED_QUAD;
typedef unsigned long UNSIGNED_QUAD;

extern UNSIGNED_BYTE get_unsigned_byte (FILE *);

extern SIGNED_BYTE get_signed_byte (FILE *);

extern UNSIGNED_PAIR get_unsigned_pair (FILE *);

extern SIGNED_PAIR get_signed_pair (FILE *);

extern UNSIGNED_TRIPLE get_unsigned_triple (FILE *);

extern SIGNED_TRIPLE get_signed_triple (FILE *);

extern SIGNED_QUAD get_signed_quad (FILE *);

extern UNSIGNED_QUAD get_unsigned_quad (FILE *);

#define ROUND(n,acc) (floor(((double)n)/(acc)+0.5)*(acc)) 

typedef signed long fixword;

extern SIGNED_QUAD sqxfw (SIGNED_QUAD sq, fixword fw);
extern SIGNED_QUAD axboverc (SIGNED_QUAD n1, SIGNED_QUAD n2,
			     SIGNED_QUAD div);

#endif /* NUMBERS_H */

