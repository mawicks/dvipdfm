#include <stdio.h>
#include "pdfobj.h"
#include "type1.h"

static pdf_obj *fontfile, *fontfile_label;

int main (int argc, char *argv[]) 
{
  if (argc < 1) {
    fprintf (stderr, "No filename to open\n");
    return 1;
  }

  fprintf (stdout, "Writing to %s\n", argv[1]);
  pdf_doc_init (argv[1]);
  /*    fontfile = type1_fontfile ("cmr10"); */
    fontfile = type1_fontfile ("c0649bt_");
    fontfile_label = pdf_ref_obj (fontfile);
    pdf_release_obj (fontfile);
    pdf_doc_new_page (200, 300);
    pdf_doc_add_to_page ("0 g 72 72 m 100 100 l 100 72 l s", 32);
    pdf_doc_new_page (612, 792);
    pdf_doc_add_to_page ("0 g 72 72 m 500 200 l 600 500 l 100 400 l b", 43);

  pdf_doc_finish();

  return 0;
}
