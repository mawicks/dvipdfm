/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/Attic/doctest.c,v 1.2 1998/11/18 02:31:32 mwicks Exp $

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
#include <math.h>
#include "pdfobj.h"
#include "type1.h"

static pdf_obj *font_resource, *fontlist_dict;

static pdf_obj *junk;

char *message = "()()\n\r\t\b\f\\()Computer Modern Roman Fonts ";
char buffer[256];

int main (int argc, char *argv[]) 
{
  pdf_obj *tmp1;
  double theta, s, c;
  double R = 144.0;
  int i;
  if (argc < 1) {
    fprintf (stderr, "No filename to open\n");
    return 1;
  }

  fprintf (stdout, "Writing to %s\n", argv[1]);


  pdf_doc_init (argv[1]);
       pdf_doc_new_page (612, 792);

junk = pdf_new_string (message, strlen(message));
 pdf_ref_obj (junk); pdf_release_obj (junk);
  

       fontlist_dict = pdf_new_dict ();
       font_resource = type1_font_resource ("cmr10", "F1");
       pdf_add_dict (fontlist_dict, tmp1 = pdf_new_name ("F1"), 
		     font_resource);
       pdf_release_obj (tmp1);
       pdf_release_obj (font_resource);
       pdf_doc_add_to_page_resources ("Font", fontlist_dict);

#define STRING0 " 1 0 0 1 312 396 cm "
       pdf_doc_add_to_page (STRING0, strlen(STRING0));
       pdf_doc_add_to_page (" BT ", 4);
#define STRING1 " /F1 32 Tf "
#define STRING2 " (\n\r\t\b\f\\()Computer Modern Roman) Tj  "
       pdf_doc_add_to_page (STRING1, strlen(STRING1));
       pdf_doc_add_to_page (STRING2, strlen(STRING2));
       pdf_doc_add_to_page (" ET ", 4);
              
    pdf_doc_new_page (612, 792);
    pdf_doc_add_to_page ("0 g 72 72 m 500 200 l 600 500 l 100 400 l b", 43);

  pdf_doc_finish();

  return 0;
}
