#include <stdio.h>
#include "pdfobj.h"
#include "type1.h"

static pdf_obj *font_resource, *fontlist_dict;

int main (int argc, char *argv[]) 
{
  pdf_obj *tmp1;
  if (argc < 1) {
    fprintf (stderr, "No filename to open\n");
    return 1;
  }

  fprintf (stdout, "Writing to %s\n", argv[1]);

  pdf_doc_init (argv[1]);
    pdf_doc_new_page (612, 792);
       fontlist_dict = pdf_new_dict ();
       font_resource = type1_font_resource ("cmr10", "F1");
       pdf_add_dict (fontlist_dict, tmp1 = pdf_new_name ("F1"), 
		     font_resource);
       pdf_release_obj (tmp1);
       pdf_release_obj (font_resource);
       pdf_doc_add_to_page_resources ("Font", fontlist_dict);
   
#define PAGE1 "BT /F1 36 Tf 0.71 0.71 -0.71 0.71 144 144 Tm [ (Computer) -433  (Modern) -433 (Roman) -433 (Font) ] TJ ET 0 g 72 72 m 100 100 l 100 72 l s "
       pdf_doc_add_to_page (PAGE1, strlen(PAGE1));
    pdf_doc_add_to_page ("0 g 72 72 m 100 100 l 100 72 l s", 32);
    pdf_doc_new_page (612, 792);
    pdf_doc_add_to_page ("0 g 72 72 m 500 200 l 600 500 l 100 400 l b", 43);

  pdf_doc_finish();

  return 0;
}
