#include <stdio.h>
#include "pdfobj.h"
#include "pdfdev.h"

int main (int argc, char *argv[]) 
{
  char *message = "Now is the time";
  int i;
    
  if (argc < 1) {
    fprintf (stderr, "No filename to open\n");
    return 1;
  }
  dev_set_debug();
  

  fprintf (stdout, "Writing to %s\n", argv[1]);
  dev_init (argv[1]);

  dev_locate_font ("cmr10", 0x12345678ul, 10.0);
  dev_locate_font ("cmr10", 0x87654321ul, 20.0);
  dev_locate_font ("cmss17", 0x11111111ul, 17.0);
  dev_bop();

  dev_moveto (100, 300);
  dev_select_font (0x12345678ul);
  
  for (i=0; i<strlen(message); i++) {
    dev_set_char (message[i], 12.0);
  }

  dev_moveright (10.11111);
  dev_select_font (0x87654321ul);
  for (i=0; i<strlen(message); i++) {
    dev_set_char (message[i], 12.0);
  }

  dev_eop();
  dev_bop();

  dev_moveto (300, 300);
  dev_select_font (0x11111111ul);
  for (i=0; i<strlen(message); i++) {
    dev_set_char (message[i], 12.0);
  }
  dev_moveto (100.0, 500.0);
  dev_rule (72.0, 72.0);
  for (i=strlen(message); i>= 0; i--) {
    dev_set_char (message[i], 12.0);
  }
  dev_eop();
  dev_close ();
  return 0;
}
