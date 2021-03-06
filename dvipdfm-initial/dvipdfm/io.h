/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/io.h,v 1.3 1998/11/21 20:18:58 mwicks Exp $

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
#include "numbers.h"

UNSIGNED_BYTE read_byte (FILE *);

void seek_absolute (FILE *file, long pos);
void seek_relative (FILE *file, long pos);

void seek_end (FILE *file);

long tell_position (FILE *file);

long file_size (FILE *file);

char *mfgets (char *buffer, long size, FILE *file);

extern char work_buffer[];
#define WORK_BUFFER_SIZE 1024
