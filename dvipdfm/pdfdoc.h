/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdoc.h,v 1.10 1998/12/12 03:35:12 mwicks Exp $

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

	
#ifndef PDFDOC_H
#define PDFDOC_H

#include "pdfobj.h"
extern void pdf_doc_new_page (void);
extern pdf_obj *pdf_doc_this_page_ref (void);
extern pdf_obj *pdf_doc_this_page (void);
extern pdf_obj *pdf_doc_page_tree (void);
extern pdf_obj *pdf_doc_catalog (void);
extern void pdf_doc_creator (char *s);
extern pdf_obj *pdf_doc_names (void);
extern pdf_obj *pdf_doc_next_page_ref (void);
extern pdf_obj *pdf_doc_prev_page_ref (void);
extern pdf_obj *pdf_doc_ref_page (unsigned long page_no);

extern void pdf_doc_add_to_page_resources (const char *name, pdf_obj
				    *resources);
extern void pdf_doc_add_to_page_fonts (const char *name, pdf_obj
				*font);
extern void pdf_doc_add_to_page_xobjects (const char *name, pdf_obj
				    *xobject);
extern pdf_obj *pdf_doc_current_page_resources(void);

extern void pdf_doc_add_to_page_annots (pdf_obj *annot);

extern void pdf_doc_add_dest (char *name, unsigned length, pdf_obj *array);

extern void pdf_doc_start_article (char *name, pdf_obj *info);
extern void pdf_doc_add_bead (char *name, pdf_obj *partial_dict);

extern void pdf_doc_merge_with_docinfo (pdf_obj *dictionary);
extern void pdf_doc_merge_with_catalog (pdf_obj *dictionary);

extern void pdf_doc_add_to_page (char *buffer, unsigned length);

extern void pdf_doc_change_outline_depth (int new_depth);
extern void pdf_doc_add_outline (pdf_obj *dict);

extern void pdf_doc_set_page_size (double width, double height);
     
extern void pdf_doc_init (char *filename);

extern void pdf_doc_comment (char *comment);
     
extern void pdf_doc_finish (void);

extern void pdf_doc_setverbose();
extern void pdf_doc_setdebug();

extern void pdf_doc_bop (char *string, unsigned length);
extern void pdf_doc_eop (char *string, unsigned length);
extern void pdf_doc_this_bop (char *string, unsigned length);
extern void pdf_doc_this_eop (char *string, unsigned length);

extern void doc_make_form_xobj (pdf_obj *stream, pdf_obj *bbox, pdf_obj
			 *resources);
extern pdf_obj *begin_form_xobj (double bbllx, double bblly, double bburx,
			  double bbury);
extern void end_form_xobj (void);

#endif /* PDFDOC_H */

