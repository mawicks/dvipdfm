#ifndef PDFDOC_H
#define PDFDOC_H
#include "pdfobj.h"

void pdf_doc_new_page (double width, double height);

pdf_obj *pdf_doc_this_page (void);
pdf_obj *pdf_doc_ref_page (unsigned page_no);

void pdf_doc_add_to_page_resources (const char *name, pdf_obj
				    *resources);
void pdf_doc_add_to_page_annots (pdf_obj *annot);

void pdf_doc_add_dest (char *name, unsigned length, pdf_obj *array);

pdf_obj *pdf_doc_add_article (char *name, pdf_obj *info);
void pdf_doc_add_bead (char *name, pdf_obj *partial_dict);

void pdf_doc_merge_with_docinfo (pdf_obj *dictionary);
void pdf_doc_merge_with_catalog (pdf_obj *dictionary);

void pdf_doc_add_to_page (char *buffer, unsigned length);

void pdf_doc_change_outline_depth (int new_depth);
void pdf_doc_add_outline (pdf_obj *dict);

void pdf_doc_set_page_size (double width, double height);
     
void pdf_doc_init (char *filename) ;
     
void pdf_doc_finish (void);

void pdf_doc_setverbose();
void pdf_doc_setdebug();

void pdf_doc_bop (char *string, unsigned length);
void pdf_doc_eop (char *string, unsigned length);



#endif /* PDFDOC_H */

