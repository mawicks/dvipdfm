#ifndef TYPE1_H
#define TYPE1_H

#include "pdfobj.h"

pdf_obj *type1_fontfile (const char *tex_name);
pdf_obj *type1_font_descriptor (const char *tex_name);
pdf_obj *type1_font_resource (const char *tex_name, const char *resource_name);

#endif /* TYPE1_H */
