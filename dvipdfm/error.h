/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/error.h,v 1.10 2001/04/14 03:25:00 mwicks Exp $

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

	
#ifndef _ERROR_H_
#define _ERROR_H_

#include "system.h"

extern void error_cleanup();

#define FATAL_ERROR -1
#define NO_ERROR 0

#define ERROR(string) { \
  fprintf(stderr, "\n"); \
  fprintf (stderr,  string); \
  fprintf (stderr, "\n");  \
  error_cleanup(); \
  exit(1);}
typedef int error_t;

#endif /* _ERROR_H_ */

