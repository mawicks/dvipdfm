#ifndef PDFSPECIAL_H

#define PDFSPECIAL_H
#include "numbers.h"

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size);
void pdf_finish_specials(void);

void pdf_open (char *name);


#endif  /* PDFSPECIAL_H */
