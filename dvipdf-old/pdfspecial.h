#ifndef PDFSPECIAL_H

#define PDFSPECIAL_H
#include "numbers.h"
#include "pdfobj.h"

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user, double x_media, double
		       y_media);
void pdf_finish_specials(void);
pdf_obj *pdf_include_page (pdf_obj *trailer, double x_user, double
		       y_user, double width, double height);
pdf_obj *get_reference(char **start, char *end);

#endif  /* PDFSPECIAL_H */
