/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/io.c,v 1.3 1998/11/21 20:18:58 mwicks Exp $

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

	

#include "io.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

static void os_error()
{
  ERROR ("io:  An OS command failed that should not have.\n");
}

UNSIGNED_BYTE read_byte (FILE *file) 
{
  int byte;
  if (feof(file)) {
    ERROR ("File ended prematurely");
  }
  if ((byte = fgetc (file)) < 0) {
    ERROR ("read_byte:  OS error");
  }
  return (UNSIGNED_BYTE) byte;
}

void seek_absolute (FILE *file, long pos) 
{
  if (fseek(file, pos, SEEK_SET)) {
    os_error();
  }
}

void seek_relative (FILE *file, long pos)
{
  if (fseek(file, pos, SEEK_CUR)) {
    os_error();
  }
}


void seek_end (FILE *file) 
{
  if (fseek(file, 0L, SEEK_END)) {
    os_error();
  }
}

long tell_position (FILE *file) 
{
  long size;
  if ((size = ftell (file)) < 0) {
    os_error();
  }
  return size;
}

long file_size (FILE *file)
{
  long size;
  /* Seek to end */
  seek_end (file);
  size = tell_position (file);
  rewind (file);
  return (size);
}

/* Unlike fgets, mfgets works with \r, \n, or \r\n end of lines. */
char *mfgets (char *buffer, long length, FILE *file) 
{
  int ch, i = 0;
  while (i < length-1 && (ch = fgetc (file)) >= 0 && ch != '\n' && ch != '\r')
    buffer[i++] = ch;
  buffer[i] = 0;
  if (ch < 0 && i == 0)
    return NULL;
  if (ch == '\r' && (ch = fgetc (file)) >= 0 && (ch != '\n'))
    ungetc (ch, file);
  return buffer;
}



char work_buffer[WORK_BUFFER_SIZE];

