/*
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

	
#include "numbers.h"
#include "error.h"
#include "io.h"


UNSIGNED_BYTE get_unsigned_byte (FILE *file)
{
  UNSIGNED_BYTE byte;
  byte = read_byte (file);
  return byte;
}

SIGNED_BYTE get_signed_byte (FILE *file)
{
  int byte;
  byte = read_byte(file);
  if (byte >= 0x80) 
    byte -= 0x100;
  return (SIGNED_BYTE) byte;
}

UNSIGNED_PAIR get_unsigned_pair (FILE *file)
{
  UNSIGNED_BYTE byte;
  int i;
  unsigned pair = 0;
  for (i=0; i<2; i++) {
    byte = read_byte (file);
    pair = pair*0x100u + byte;
  }
  return pair;
}

SIGNED_PAIR get_signed_pair (FILE *file)
{
  int byte, i;
  long pair = 0;
  for (i=0; i<2; i++) {
    byte = read_byte (file);
    pair = pair*0x100 + byte;
  }
  if (pair >= 0x8000) {
    pair -= 0x10000l;
  }
  return (SIGNED_PAIR) pair;
}


UNSIGNED_TRIPLE get_unsigned_triple(FILE *file)
{
  UNSIGNED_BYTE byte;
  int i;
  long triple = 0;
  for (i=0; i<3; i++) {
    byte = read_byte(file);
    triple = triple*0x100u + byte;
  }
  return (UNSIGNED_TRIPLE) triple;
}

SIGNED_TRIPLE get_signed_triple(FILE *file)
{
  int byte, i;
  long triple = 0;
  for (i=0; i<3; i++) {
    byte = read_byte(file);
    triple = triple*0x100 + byte;
  }
  if (triple >= 0x800000l) 
    triple -= 0x1000000l;
  return (SIGNED_TRIPLE) triple;
}

SIGNED_QUAD get_signed_quad(FILE *file)
{
  int byte, i;
  long quad = 0;

  /* Check sign on first byte before reading others */
  byte = read_byte(file);
  quad = byte;
  if (quad >= 0x80) 
    quad = byte - 0x100;

  for (i=0; i<3; i++) {
    byte = read_byte(file);
    quad = quad*0x100 + byte;
  }

  return (SIGNED_QUAD) quad;
}

UNSIGNED_QUAD get_unsigned_quad(FILE *file)
{
  UNSIGNED_BYTE byte;
  int i;
  unsigned long quad = 0;

  for (i=0; i<4; i++) {
    byte = read_byte(file);
    quad = quad*0x100u + byte;
  }
  return (UNSIGNED_QUAD) quad;
}



