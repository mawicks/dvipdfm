/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/mem.c,v 1.3 1998/12/05 11:47:24 mwicks Exp $

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
#include <stdlib.h>
#include "mem.h"

#ifdef MEM_DEBUG
#include <time.h>
#include <unistd.h>
FILE *debugfile = NULL;
clock_t start_time = 0;
#endif MEM_DEBUG

void *new (size_t size, char *function, int line)
{
  void *result;
  if ((result = malloc (size)) == NULL) {
    fprintf (stderr, "Out of memory!\n");
    exit (1);
  }

#ifdef MEM_DEBUG  
    if (debugfile == NULL) {
      debugfile = fopen ("malloc.log", "w");
      fprintf (debugfile, "Clocks per sec: %d\n", CLOCKS_PER_SEC);
      start_time = clock();
    }
    fprintf (debugfile, "%p %010ld new %s:%d\n", result, clock()-start_time, function, line);
#endif /* MEM_DEBUG */

  return result;
}

void *renew (void *mem, size_t size, char *function, int line)
{
  void *result;
  if ((result = realloc (mem, size)) == NULL) {
    fprintf (stderr, "Out of memory!\n");
    exit (1);
  }

#ifdef MEM_DEBUG
    if (debugfile == NULL) {
      debugfile = fopen ("malloc.log", "w");
      fprintf (debugfile, "Clocks per sec: %d\n", CLOCKS_PER_SEC);
      start_time = clock();
    }
    if (mem != NULL)
      fprintf (debugfile, "%p %010ld fre %s:%d\n", mem,
	       clock()-start_time, function, line);
    fprintf (debugfile, "%p %010ld new %s:%d\n", result, clock()-start_time, function, line);
#endif /* MEM_DEBUG */

  return result;
}

void release (void *mem, char *function, int line)
{

#ifdef MEM_DEBUG
    if (debugfile == NULL) {
      debugfile = fopen ("malloc.log", "w");
      fprintf (debugfile, "Clocks per sec: %d\n", CLOCKS_PER_SEC);
      start_time = clock();
    }
    fprintf (debugfile, "%p %010ld fre %s:%d\n", mem, clock()-start_time, function, line);
#endif /* MEM_DEBUG */

  free (mem);
}

