#ifndef EPDF_H
#define EPDF_H
#include "pdfspecial.h"
#include "pdfobj.h"

pdf_obj *pdf_include_page (pdf_obj *trailer, double x_user, double
			   y_user,
			   struct xform_info *p);

#endif /* EPDF_H */
