#ifndef PDFSPECIAL_H

#define PDFSPECIAL_H
#include "numbers.h"
#include "pdfobj.h"

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user, double x_media, double y_media);
void pdf_finish_specials(void);

void pdf_open (char *name, double x_user, double y_user);

pdf_obj *pdf_read_object (int obj_no);
pdf_obj *pdf_deref_obj (pdf_obj *obj_ref);
pdf_obj *pdf_ref_file_obj (int obj_no);

#endif  /* PDFSPECIAL_H */
