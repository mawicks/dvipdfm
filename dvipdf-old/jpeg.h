#ifndef JPEG_H

#define JPEG_H

#include "pdfobj.h"

struct jpeg 
{
  unsigned width, height;
  unsigned bits_per_color;
  FILE *file;
  unsigned colors;
};

struct jpeg *jpeg_open (char *filename);
void jpeg_close (struct jpeg *jpeg);
pdf_obj *jpeg_build_object(struct jpeg *jpeg,
			   double x_user, double y_user,
			   double width, double height);

#endif /* JPEG_H */
