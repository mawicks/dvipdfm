/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/epdf.c,v 1.15 1999/08/15 04:54:55 mwicks Exp $

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

#include "system.h"
#include "pdfobj.h"
#include "pdfdoc.h"
#include "pdfspecial.h"
#include "mfileio.h"
#include "epdf.h"
#include "mem.h"

double xscale, yscale;

void do_scaling(pdf_obj *media_box, struct xform_info *p)
{ 
  /* Take care of scaling */
  double bbllx, bblly, bburx, bbury;
  bbllx = pdf_number_value (pdf_get_array (media_box, 0));
  bblly = pdf_number_value (pdf_get_array (media_box, 1));
  bburx = pdf_number_value (pdf_get_array (media_box, 2));
  bbury = pdf_number_value (pdf_get_array (media_box, 3));
  xscale = 1.0;
  yscale = 1.0;
  if (p->scale != 0.0) {
    xscale = p->scale;
    yscale = p->scale;
  }
  if (p->xscale != 0.0) {
    xscale = p->xscale;
  }
  if (p->yscale != 0.0) {
    yscale = p->yscale;
  }
  if (p-> width != 0.0 && bbllx != bburx) {
    xscale = p->width / (bburx - bbllx);
    if (p->height == 0.0)
      yscale = xscale;
  }
  if (p->height != 0.0 && bblly != bbury) {
    yscale = p->height / (bbury - bblly);
    if (p->width == 0.0)
      xscale = yscale;
  }
  p->xscale = xscale;
  p->yscale = yscale;
}

pdf_obj *pdf_include_page(FILE *image_file, struct xform_info *p, char *res_name)
{
  pdf_obj *trailer = NULL, *catalog = NULL, *page_tree = NULL;
  pdf_obj *kids_ref, *kids;
  pdf_obj *media_box = NULL, *crop_box = NULL, *resources = NULL,
    *contents = NULL, *contents_ref = NULL;
  pdf_obj *tmp1;
#ifdef MEM_DEBUG
MEM_START
#endif
  if (!(trailer = pdf_open (image_file))) {
    fprintf (stderr, "\nCorrupt PDF file?\n");
  }
 
  /* Now just lookup catalog location */
  /* Deref catalog */
  if (trailer && (catalog = pdf_deref_obj(pdf_lookup_dict (trailer,"Root"))) ==
      NULL) {
    fprintf (stderr, "\nCatalog isn't where I expect it.\n");
  }
  if (trailer)
    pdf_release_obj (trailer);

  /* Lookup page tree in catalog */
  if (catalog) {
    page_tree = pdf_deref_obj (pdf_lookup_dict (catalog, "Pages"));
    /* Should be finished with catalog */
    pdf_release_obj (catalog);
  }
  /* Media box and resources can be inherited so start looking for
     them here */
  if (page_tree) {
    if ((tmp1 = pdf_lookup_dict (page_tree, "CropBox")))
      crop_box = pdf_deref_obj (tmp1);
    if ((tmp1 = pdf_lookup_dict (page_tree, "MediaBox")))
      media_box = pdf_deref_obj (tmp1);
    resources = pdf_deref_obj (pdf_lookup_dict (page_tree, "Resources"));
    if (resources == NULL) {
      resources = pdf_new_dict();
    }
    while ((kids_ref = pdf_lookup_dict (page_tree, "Kids")) != NULL) {
      kids = pdf_deref_obj (kids_ref);
      pdf_release_obj (page_tree);
      page_tree = pdf_deref_obj (pdf_get_array(kids, 0));
      pdf_release_obj (kids);
      /* Replace MediaBox if it's here */
      tmp1 = pdf_deref_obj(pdf_lookup_dict (page_tree, "MediaBox"));
      if (tmp1 && media_box)
	pdf_release_obj (media_box);
      if (tmp1) 
	media_box = tmp1;
      /* Do same for CropBox */
      tmp1 = pdf_deref_obj(pdf_lookup_dict (page_tree, "CropBox"));
      if (tmp1 && crop_box)
	pdf_release_obj (crop_box);
      if (tmp1) 
	crop_box = tmp1;
      /* Add resources if they're here */
      tmp1 = pdf_deref_obj (pdf_lookup_dict (page_tree, "Resources"));
      if (tmp1) {
	pdf_merge_dict (tmp1, resources);
	pdf_release_obj (resources);
	resources = tmp1;
      }
    }
    /* At this point, page_tree contains the first page.  media_box,
       crop_box,  and resources should also be set. */
    /* If there's a crop_box, replace media_box with crop_box.
       The rest of this routine assumes crop_box has been released */
    if (crop_box) {
      pdf_release_obj (media_box);
      media_box = crop_box;
      crop_box = NULL;
    }
    /* Adjust scaling information as necessary */
    do_scaling (media_box, p);
    if ((contents =
	 pdf_deref_obj(pdf_lookup_dict(page_tree,"Contents")))==NULL) {
      fprintf (stderr, "\nNo Contents found\n");
      return NULL;
    }
    pdf_release_obj (page_tree);
  }
  /* Arrays of contents must be handled very differently (not implemented) */
  if (contents && contents -> type != PDF_ARRAY) {
    doc_make_form_xobj (contents, media_box, resources, res_name);
  } else {
    fprintf (stderr, "\nIgnoring stream with with multiple segments\n");
    contents = NULL;
  }
  if (contents) {
    contents_ref = pdf_ref_obj (contents);
    pdf_release_obj (contents);
  }
  pdf_close ();
#ifdef MEM_DEBUG
MEM_END
#endif
  return (contents_ref);
}
