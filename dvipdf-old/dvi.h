
#include "error.h"

error_t dvi_open (char *filename);

void dvi_set_verbose (void);
void dvi_set_debug (void);

void dvi_close (void);  /* Closes data structures created by dvi_open */
void dvi_complete (void);  /* Closes output file being written by an
			      actual driver */

void dvi_init (char *outputfile);

double dvi_tell_mag (void);






