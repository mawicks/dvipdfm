/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/Attic/devtest.c,v 1.2 1998/11/18 02:31:32 mwicks Exp $

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
#include "pdfobj.h"
#include "pdfdev.h"

int main (int argc, char *argv[]) 
{
  char *message = "Now is the time";
  int i;
    
  if (argc < 1) {
    fprintf (stderr, "No filename to open\n");
    return 1;
  }
  dev_set_debug();
  

  fprintf (stdout, "Writing to %s\n", argv[1]);
  dev_init (argv[1]);

  dev_locate_font ("cmr10", 0x12345678ul, 10.0);
  dev_locate_font ("cmr10", 0x87654321ul, 20.0);
  dev_locate_font ("cmss17", 0x11111111ul, 17.0);
  dev_bop();

  dev_moveto (100, 300);
  dev_select_font (0x12345678ul);
  
  for (i=0; i<strlen(message); i++) {
    dev_set_char (message[i], 12.0);
  }

  dev_moveright (10.11111);
  dev_select_font (0x87654321ul);
  for (i=0; i<strlen(message); i++) {
    dev_set_char (message[i], 12.0);
  }

  dev_eop();
  dev_bop();

  dev_moveto (300, 300);
  dev_select_font (0x11111111ul);
  for (i=0; i<strlen(message); i++) {
    dev_set_char (message[i], 12.0);
  }
  dev_moveto (100.0, 500.0);
  dev_rule (72.0, 72.0);
  for (i=strlen(message); i>= 0; i--) {
    dev_set_char (message[i], 12.0);
  }
  dev_eop();
  dev_close ();
  return 0;
}
