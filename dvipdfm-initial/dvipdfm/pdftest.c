/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/Attic/pdftest.c,v 1.2 1998/11/18 02:31:34 mwicks Exp $

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

int main (int argc, char *argv[]) 
{
  pdf_obj *typeobj, *pagecount, *pagecount_label, *catalog, *pagetree,
    *pagetree_label, *thispage, *thispage_label, *pageskids;
  pdf_obj *resources, *procset, *procset_label;
  pdf_obj *thispagecontents, *thispagecontents_label;
  pdf_obj *pagekids, *pageskids_label;

  pdf_obj *tmp1, *tmp2;

  if (argc < 1) {
    fprintf (stderr, "No filename to open\n");
    return 1;
    
  }

  fprintf (stdout, "Writing to %s\n", argv[1]);
  pdf_out_init (argv[1]);

  /* Following is for convenience */
  typeobj = pdf_new_name ("Type");

  catalog = pdf_new_dict ();
  pdf_set_root (catalog);
  /* Create /Type attribute */
  pdf_add_dict (catalog,
		typeobj,
		tmp1 = pdf_new_name("Catalog"));
  pdf_release_obj (tmp1);
  /* Create empty page tree */
  pagetree = pdf_new_dict();
  pagetree_label = pdf_ref_obj (pagetree);
  /* Point /Pages attribute to indirect reference since we
     haven't built the tree yet */
  pdf_add_dict (catalog,
		tmp1 = pdf_new_name ("Pages"),
		pagetree_label);
  pdf_release_obj (tmp1);
  /* That's it, the catalog's done.  Ship it out! */
  pdf_release_obj (catalog);

  /* work on resources to put in Pages */
  resources = pdf_new_dict ();
  procset = pdf_new_array ();
  procset_label = pdf_ref_obj (procset);
  pdf_add_dict (resources, tmp1 = pdf_new_name ("ProcSet"),
		procset_label);
  pdf_release_obj (tmp1);
  pdf_release_obj (procset_label);
    
  pdf_add_array (procset, tmp1 = pdf_new_name ("PDF"));
  pdf_release_obj (tmp1);
  pdf_add_array (procset, tmp1 = pdf_new_name ("ImageB"));
  pdf_release_obj (tmp1);
  pdf_add_array (procset, tmp1 = pdf_new_name ("Text"));
  pdf_release_obj (procset);
  
  /* Back to work on that page tree */
  pdf_add_dict (pagetree,
		typeobj,
		tmp1 = pdf_new_name ("Pages"));
  pdf_ref_obj (tmp1);
  pdf_release_obj (tmp1);
  pagecount = pdf_new_number (0);
  pagecount_label = pdf_ref_obj (pagecount);
  pdf_add_dict (pagetree,
		tmp1 = pdf_new_name ("Count"),
		pagecount_label);
  pdf_release_obj (tmp1);
  pageskids = pdf_new_array ();
  pageskids_label = pdf_ref_obj (pageskids);
  
  pdf_add_dict (pagetree,
		tmp1 = pdf_new_name ("Kids"),
		pageskids_label);
  pdf_release_obj (tmp1);
  pdf_add_dict (pagetree,
		tmp1 = pdf_new_name ("Resources"),
		resources);
  pdf_release_obj (tmp1);
  pdf_release_obj (resources);
  /* Pagetree is done.  */
  pdf_release_obj (pagetree);

  /* From now on, simply add pages to pageskids as they are finished */

  /* ********* MAIN PAGE BUILDING LOOP ******* */

  thispage = pdf_new_dict ();
  thispage_label = pdf_ref_obj (thispage);
  pdf_add_array (pageskids,
		 thispage_label);
  pdf_add_dict (thispage,
		typeobj,
		tmp1 = pdf_new_name ("Page"));
  pdf_release_obj (tmp1);

  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (200)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (300)); pdf_release_obj (tmp2);
  pdf_add_dict (thispage,
		tmp2 = pdf_new_name ("MediaBox"),
		tmp1);
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  pdf_add_dict (thispage, tmp1 = pdf_new_name ("Parent"),
		pagetree_label);
  thispagecontents = pdf_new_stream();
  thispagecontents_label = pdf_ref_obj (thispagecontents);
  pdf_add_dict (thispage, tmp1 = pdf_new_name ("Contents"),
		thispagecontents_label);
  /* Flush this page */
  pdf_release_obj (thispage);

  /* Add content to "Contents" */

  pdf_add_stream (thispagecontents, "0 g 72 72 m 100 100 l 100 72 l s",
		  32);

  /* Flush the Content stream */

  pdf_release_obj (thispagecontents);
  
  /* ********* MAIN PAGE BUILDING LOOP ******* */
  

  pdf_set_number (pagecount, 1);
  pdf_release_obj (pagecount);
  pdf_release_obj (pageskids);
  pdf_release_obj (pagetree_label);
  pdf_release_obj (pageskids_label);
    
  pdf_out_flush ();

  return 0;
}
