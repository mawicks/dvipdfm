#ifndef _EPDF_H_
#define _EPDF_H_

#include <stdio.h>
#include "pdfspecial.h"
#include "pdfobj.h"

extern pdf_obj *pdf_include_page (pdf_obj *trailer, double x_user, double
			   y_user,
			   struct xform_info *p);

#endif /* _EPDF_H_ */
