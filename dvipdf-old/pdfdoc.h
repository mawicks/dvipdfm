#ifndef PDFDOC_H
#define PDFDOC_H
#include "pdfobj.h"

void pdf_doc_new_page (unsigned width, unsigned length); /* Units are
							   points */
void pdf_doc_add_to_page_resources (const char *name, pdf_obj
				    *resources);

void pdf_doc_add_to_page (char *buffer, unsigned length);
     
void pdf_doc_init (char *filename) ;
     
void pdf_doc_finish (void);

#endif /* PDFDOC_H */
