#include <stdio.h>
#include "dvi.h"

int main (int argc, char *argv[]) 
{
  if (argc < 1) {
    fprintf (stderr, "No filename to open\n");
    return 1;
    
  }

  dvi_set_verbose();
  fprintf (stdout, "Opening %s\n", argv[1]);
  dvi_open (argv[1]);
  return 0;

}
