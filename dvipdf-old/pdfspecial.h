#ifndef PDFSPECIAL_H

#define PDFSPECIAL_H
#include "numbers.h"
#include "pdfobj.h"

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size);
void pdf_finish_specials(void);

void pdf_open (char *name);

pdf_obj *pdf_deref_obj (pdf_obj *obj_ref);

#endif  /* PDFSPECIAL_H */
