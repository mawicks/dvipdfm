#include <stdio.h>
#include "tfm.h"

int main (int argc, char *argv[]) 
{
  if (argc < 2) {
    fprintf (stderr, "No filename to open\n");
    return 1;
  }

  tfm_set_verbose();
  fprintf (stdout, "Opening %s\n", argv[1]);
  tfm_open (argv[1]);
  return 0;
}

