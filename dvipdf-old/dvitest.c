#include <stdio.h>
#include "dvi.h"
#include "kpathsea/progname.h"

int main (int argc, char *argv[]) 
{
  int i;
  
  if (argc < 3) {
    fprintf (stderr, "Usage:  dvipdf  inputfile outputfile");
    return 1;
  }
  kpse_set_progname (argv[0]);
  /*   dvi_set_debug(); 
     dvi_set_verbose(); */
  /*   tfm_set_verbose(); */
  /*   tfm_set_debug(); */
  /*   pdf_doc_set_verbose();
       pdf_doc_set_debug(); */
  /*  dev_set_verbose();
      dev_set_debug(); */
  
  fprintf (stdout, "Reading %s\n", argv[1]);
  dvi_open (argv[1]);
  fprintf (stdout, "Writing to %s\n", argv[2]);
  dvi_init (argv[2]);
  for (i=0; i<dvi_npages(); i++) {
    fprintf (stderr, "[%d", i+1);
    dvi_do_page (i);
    fprintf (stderr, "]");
  }
  dvi_complete ();
  dvi_close();
  fprintf (stderr, "\n");
  return 0;

}
