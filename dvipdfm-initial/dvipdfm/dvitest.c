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

	
#include <stdio.h>
#include "dvi.h"
#include "kpathsea/progname.h"

int main (int argc, char *argv[]) 
{
  int i;
  
  if (argc < 3) {
    fprintf (stderr, "Usage:  dvipdf  inputfile outputfile");
    return 1;
  }
  kpse_set_progname (argv[0]);
  /*   dvi_set_debug(); 
     dvi_set_verbose(); */
  /*   tfm_set_verbose(); */
  /*   tfm_set_debug(); */
  /*   pdf_doc_set_verbose();
       pdf_doc_set_debug(); */
  /*  dev_set_verbose();
      dev_set_debug(); */
  
  fprintf (stdout, "Reading %s\n", argv[1]);
  dvi_open (argv[1]);
  fprintf (stdout, "Writing to %s\n", argv[2]);
  dvi_init (argv[2]);
  for (i=0; i<dvi_npages(); i++) {
    fprintf (stderr, "[%d", i+1);
    dvi_do_page (i);
    fprintf (stderr, "]");
  }
  dvi_complete ();
  dvi_close();
  fprintf (stderr, "\n");
  return 0;

}
