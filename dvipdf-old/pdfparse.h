#ifndef PDFPARSE_H

#define PDFPARSE_H
#include "numbers.h"
#include "pdfobj.h"

int is_a_number(const char *s);

char *parse_ident (char **start, char *end);
char *parse_number (char **start, char*end);
void parse_crap (char **start, char *end);
void skip_white (char **start, char *end);
void skip_line (char **start, char *end);

pdf_obj *parse_pdf_string (char **start, char*end);
pdf_obj *parse_pdf_name (char **start, char*end);
pdf_obj *parse_pdf_array (char **start, char*end);
pdf_obj *parse_pdf_object (char **start, char*end);
pdf_obj *parse_pdf_dict (char **start, char*end);
pdf_obj *parse_pdf_ident (char **start, char*end);
pdf_obj *parse_pdf_boolean (char **start, char*end);
pdf_obj *parse_pdf_null (char **start, char*end);
char *parse_pdf_reference (char **start, char*end);
char *parse_opt_ident (char **start, char*end);

void pdf_finish_specials(void);
pdf_obj *pdf_read_object (int obj_no);

void dump (char *start, char *end);

#endif  /* PDFPARSE_H */
