#ifndef PDFSPECIAL_H

#define PDFSPECIAL_H
#include "numbers.h"
#include "pdfobj.h"

void pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user, double x_media, double
		       y_media);
pdf_obj *parse_pdf_dict (char **start, char*end);
pdf_obj *parse_pdf_object (char **start, char*end);
char *parse_number (char **start, char*end);
void parse_crap (char **start, char *end);
void skip_white (char **start, char *end);
void skip_line (char **start, char *end);

void pdf_finish_specials(void);

pdf_obj *pdf_open (char *name);
void pdf_include_page (pdf_obj *trailer, double x_user, double y_user);
void pdf_close (void);

pdf_obj *pdf_read_object (int obj_no);
pdf_obj *pdf_deref_obj (pdf_obj *obj_ref);
pdf_obj *pdf_ref_file_obj (int obj_no);

#endif  /* PDFSPECIAL_H */
