/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdoc.h,v 1.3 1998/12/03 02:40:39 mwicks Exp $

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
void pdf_doc_new_page (void);
pdf_obj *pdf_doc_this_page_ref (void);
pdf_obj *pdf_doc_this_page (void);
pdf_obj *pdf_doc_page_tree (void);
pdf_obj *pdf_doc_names (void);
pdf_obj *pdf_doc_next_page_ref (void);
pdf_obj *pdf_doc_prev_page_ref (void);
pdf_obj *pdf_doc_ref_page (unsigned page_no);

void pdf_doc_add_to_page_resources (const char *name, pdf_obj
				    *resources);
pdf_obj *pdf_doc_current_page_resources(void);

void pdf_doc_add_to_page_annots (pdf_obj *annot);

void pdf_doc_add_dest (char *name, unsigned length, pdf_obj *array);

void pdf_doc_start_article (char *name, pdf_obj *info);
void pdf_doc_add_bead (char *name, pdf_obj *partial_dict);

void pdf_doc_merge_with_docinfo (pdf_obj *dictionary);
void pdf_doc_merge_with_catalog (pdf_obj *dictionary);

void pdf_doc_add_to_page (char *buffer, unsigned length);

void pdf_doc_change_outline_depth (int new_depth);
void pdf_doc_add_outline (pdf_obj *dict);

void pdf_doc_set_page_size (double width, double height);
     
void pdf_doc_init (char *filename);

void pdf_doc_comment (char *comment);
     
void pdf_doc_finish (void);

void pdf_doc_setverbose();
void pdf_doc_setdebug();

void pdf_doc_bop (char *string, unsigned length);
void pdf_doc_eop (char *string, unsigned length);
void pdf_doc_this_bop (char *string, unsigned length);
void pdf_doc_this_eop (char *string, unsigned length);

#endif /* PDFDOC_H */

