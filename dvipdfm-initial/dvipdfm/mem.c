/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/mem.c,v 1.3 1998/11/21 21:02:17 mwicks Exp $

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

	
#include <stdio.h>
#include "mem.h"

void *new (unsigned long size)
{
  void *result;
  if ((result = malloc (size)) == NULL) {
    fprintf (stderr, "Out of memory!\n");
    exit (1);
  }
  return result;
}

void *renew (void *mem, unsigned long size)
{
  void *result;
  if ((result = realloc (mem, size)) == NULL) {
    fprintf (stderr, "Out of memory!\n");
    exit (1);
  }
  return result;
}

void release (void *mem)
{
  free (mem);
}

