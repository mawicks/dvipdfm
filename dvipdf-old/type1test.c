#include "type1.h"
#include "error.h"

main (int argc, char *argv[])
{
  if (argc < 2){
    ERROR ("Not enough arguments");
  }
  type1_font_descriptor (argv[1]);
}




