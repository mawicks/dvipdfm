#include <stdio.h>
#include "dvi.h"
#include "kpathsea/progname.h"
#include "mem.h"
#include "string.h"
#include "config.h"
#include "pdfdoc.h"
#include "pdfdev.h"

struct rect 
{
  double width;
  double height;
};
typedef struct rect rect;

struct 
{
  char *s;
  struct rect data;
} paper_sizes[] = {
  {"letter" , { 612.0, 792.0}},
  {"legal" , { 612.0, 1008.0}},
  {"ledger" , { 1224.0, 792.0}},
  {"tabloid" , { 792.0, 1224.0}},
  {"a4" , { 595.27, 841.82}},
  {"a3" , { 841.82, 1190.16}}};

static rect get_paper_size (char *string)
{
  int i;
  for (i=0; i<sizeof(paper_sizes)/sizeof(paper_sizes[0]); i++) {
    if (!strcmp (string, paper_sizes[i].s))
      break;
  }
  if (i == sizeof(paper_sizes)/sizeof(paper_sizes[0]))
    ERROR ("Paper size is invalid");
  return paper_sizes[i].data;
}


char *dvi_filename = NULL, *pdf_filename = NULL;

static void set_default_pdf_filename(void)
{
  char *dvi_base;
  dvi_base = basename (dvi_filename);
  if (strlen (dvi_base) < 5 || strncmp (".dvi", dvi_base+strlen(dvi_base)-4, 4)) 
  {
    pdf_filename = NEW (strlen(dvi_base)+5, char);
    strcpy (pdf_filename, dvi_base);
    strcat (pdf_filename, ".pdf");
  } else
  {
    pdf_filename = NEW (strlen(dvi_base)+1, char);
    strncpy (pdf_filename, dvi_base, strlen(dvi_base)-4);
    strcpy (pdf_filename+strlen(dvi_base)-4, ".pdf");
  }
}

static void usage (void)
{
   fprintf (stderr, "%s, version %s, Copyright (C) 1998 by Mark A. Wicks\n", PACKAGE, VERSION);
   fprintf (stderr, "dvipdfm comes with ABSOLUTELY NO WARRANTY.\n");
   fprintf (stderr, "This is free software, and you are welcome to redistribute it\n");
   fprintf (stderr, "under certain conditions.  Details are distributed with the software.\n");
   fprintf (stderr, "\nUsage: dvipdf [options] dvifile\n");
   fprintf (stderr, "where [options] is one or more of\n\n");
   fprintf (stderr, "\t-o filename\t(Output file name)\n");
   fprintf (stderr, "\t-p papersize\t(e.g., letter, tabloid, ledger, tabloid, a4, a3)\n");
   fprintf (stderr, "\t-l \t\t(landscape mode)\n");
   exit(1);
}

static double paper_width = 612.0, paper_height = 792.0;
static landscape_mode = 0;

static void do_args (int argc, char *argv[])
{
  while (argc > 0) {
    if (*argv[0] == '-') {
      switch (*(argv[0]+1)) {
      case 'o':  
	if (argc < 2)
	  ERROR ("Missing output file name");
	pdf_filename = NEW (strlen(argv[1])+1,char);
	strcpy (pdf_filename, argv[1]);
	argv += 2;
	argc -= 2;
	break;
      case 'p':
	{
	  rect paper_size = get_paper_size (argv[1]);
	  if (argc < 2)
	    ERROR ("Missing paper size");
	  paper_width = paper_size.width;
	  paper_height = paper_size.height;
	  argv += 2;
	  argc -= 2;
	}
	break;
      case 'l':
	landscape_mode = 1;
	argv += 1;
	argc -= 1;
	break;
      default:
	usage();
      }
    } else {
      if (dvi_filename != NULL) {
	fprintf (stderr, "Multiple filenames specified\n");
	exit(1);
      }
      if (strncmp (".dvi", argv[0]+strlen(argv[0])-4, 4)) {
	dvi_filename = NEW (strlen (argv[0])+1+4, char);
	strcpy (dvi_filename, argv[0]);
	strcat (dvi_filename, ".dvi");
      }
      else {
	dvi_filename = NEW (strlen (argv[0])+1, char);
	strcpy (dvi_filename, argv[0]);
      }
      argv += 1;
      argc -= 1;
    }
  }
  if (dvi_filename == NULL) {
    fprintf (stderr, "No dvi filename specified\n");
    exit(1);
  }
}


int main (int argc, char *argv[]) 
{
  int i;
  static int really_quiet = 0;
  if (argc < 2) {
    usage();
    return 1;
  }
  kpse_set_progname (argv[0]);
  argv+=1;
  argc-=1;
  do_args (argc, argv);

  /* Check for ".dvi" at end of argument name */
  if (pdf_filename == NULL)
    set_default_pdf_filename();
  
  if (!really_quiet)
    fprintf (stdout, "%s -> %s\n", dvi_filename, pdf_filename);
  dvi_open (dvi_filename);
  dvi_init (pdf_filename);
  if (landscape_mode)
    dev_set_page_size (paper_height, paper_width);
  else
    dev_set_page_size (paper_width, paper_height);

  
  /*   dvi_set_debug(); 
     dvi_set_verbose(); */
  /*   tfm_set_verbose(); */
  /*   tfm_set_debug(); */
  /*   pdf_doc_set_verbose();
       pdf_doc_set_debug(); */
  /*  dev_set_verbose();
      dev_set_debug(); */
  
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
