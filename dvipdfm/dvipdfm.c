/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/dvipdfm.c,v 1.18 1998/12/30 20:14:17 mwicks Exp $

    This is dvipdf, a DVI to PDF translator.
    Copyright (C) 1998  by Mark A. Wicks

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
    
    The author may be contacted via the e-mail address

	mwicks@kettering.edu
*/

	
#include <stdio.h>
#include <string.h>
#include "c-auto.h"
#include <ctype.h>

#ifdef HAVE_BASENAME
#include <libgen.h>
#endif

#include "dvi.h"
#ifdef KPATHSEA
#include <kpathsea/progname.h>
#endif
#include "mem.h"
#include "pdfdoc.h"
#include "pdfdev.h"
#include "type1.h"
#include "pdfspecial.h"
#include "pdfparse.h"
#include "vf.h"

#ifndef HAVE_BASENAME /* If system doesn't have basename, kpath library does */
const char *basename (const char *s);
#endif /* HAVE_BASENAME */

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
  const char *dvi_base;
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
   fprintf (stderr, "\t-c      \tIgnore color specials (for printing on B&W printers)\n");
   fprintf (stderr, "\t-f filename\tSet font map file name [pdffonts.map]\n");
   fprintf (stderr, "\t-o filename\tSet output file name [dvifile.pdf]\n");
   fprintf (stderr, "\t-l \t\tLandscape mode\n");
   fprintf (stderr, "\t-m number\tSet additional magnification\n");
   fprintf (stderr, "\t-p papersize\tSet papersize (letter, legal, ledger, tabloid, a4, or a3) [letter]\n");
   fprintf (stderr, "\t-x dimension\tSet horizontal offset [1.0in]\n");
   fprintf (stderr, "\t-y dimension\tSet vertical offset [1.0in]\n");
   fprintf (stderr, "\t-e          \tDisable partial font embedding [default is enabled])\n");
   fprintf (stderr, "\t-z number\tSet compression level (0-9) [default is 9])\n");
   fprintf (stderr, "\t-v          \tBe verbose\n");
   fprintf (stderr, "\t-vv         \tBe more verbose\n");
   fprintf (stderr, "\nAll dimensions entered on the command are \"true\" TeX dimensions.\n");
   exit(1);
}

static double paper_width = 612.0, paper_height = 792.0;
static int paper_specified = 0;
static landscape_mode = 0;
static ignore_colors = 0;
static double mag = 1.0, x_offset=72.0, y_offset=72.0;

#define pop_arg() {argv += 1; argc -= 1;}

static void do_args (int argc, char *argv[])
{
  char *flag;
  while (argc > 0 && *argv[0] == '-') {
    for (flag=argv[0]+1; *flag != 0; flag++) {
      switch (*flag) {
      case 'm':
	if (argc < 2) {
	  fprintf (stderr, "Magnification specification missing a number\n");
	  usage();
	}
	{
	  char *result, *end, *start = argv[1];
	  end = start + strlen(argv[1]);
	  result = parse_number (&start, end);
	  if (result != NULL && start == end) {
	    mag = atof (result);
	  }
	  else {
	    fprintf (stderr, "Error in number following magnification specification\n");
	    usage();
	  }
	  if (result != NULL) {
	    RELEASE (result);
	  }
	}
	pop_arg();
	break;
      case 'x':
	if (argc < 2) {
	  fprintf (stderr, "Magnification specification missing a number\n");
	  usage();
	}
	{
	  char *result, *end, *start = argv[1];
	  double unit;
	  end = start + strlen(argv[1]);
	  result = parse_number (&start, end);
	  if (result != NULL) {
	    x_offset = atof (result);
	  }
	  else {
	    fprintf (stderr, "Error in number following xoffset specification\n");
	    usage();
	  }
	  if (result != NULL) {
	    RELEASE (result);
	  }
	  unit = parse_one_unit(&start, end);
	  if (unit > 0.0) {
	    x_offset *= unit;
	  }
	  else {
	    fprintf (stderr, "Error in dimension specification following xoffset");
	    usage();
	  }
	}
	pop_arg();
	break;
      case 'y':
	if (argc < 2) {
	  fprintf (stderr, "Magnification specification missing a number\n");
	  usage();
	}
	{
	  char *result, *end, *start = argv[1];
	  double unit;
	  end = start + strlen(argv[1]);
	  result = parse_number (&start, end);
	  if (result != NULL) {
	    y_offset = atof (result);
	  }
	  else {
	    fprintf (stderr, "Error in number following yoffset specification\n");
	    usage();
	  }
	  if (result != NULL) {
	    RELEASE (result);
	  }
	  unit = parse_one_unit(&start, end);
	  if (unit > 0.0) {
	    y_offset *= unit;
	  }
	  else {
	    fprintf (stderr, "Error in dimension specification following yoffset");
	    usage();
	  }
	}
	pop_arg();
	break;
      case 'o':  
	if (argc < 2)
	  ERROR ("Missing output file name");
	pdf_filename = NEW (strlen(argv[1])+1,char);
	strcpy (pdf_filename, argv[1]);
	pop_arg();
	break;
      case 'p':
	{
	  rect paper_size = get_paper_size (argv[1]);
	  if (argc < 2)
	    ERROR ("Missing paper size");
	  paper_width = paper_size.width;
	  paper_height = paper_size.height;
	  paper_specified = 1;
	  pop_arg();
	}
	break;
      case 'c':
	ignore_colors = 1;
	break;
      case 'l':
	landscape_mode = 1;
	break;
      case 'f':
	type1_set_mapfile (argv[1]);
	pop_arg();
	break;
      case 'e':
	type1_disable_partial();
	break;
      case 'v':
	dvi_set_verbose();
	type1_set_verbose();
	vf_set_verbose();
	pdf_obj_set_verbose();
	pdf_doc_set_verbose();
	break;
      case 'z': 
	{
	  int level = 9;
	  if (isdigit (*(flag+1))) {
	    level = *(++flag) - '0';
	  } else {
	    char *result, *end, *start = argv[1];
	    if (argc < 2) {
	      fprintf (stderr, "\nCompression specification missing number for level\n");
	      usage();
	    }
	    end = start + strlen(argv[1]);
	    result = parse_number (&start, end);
	    if (result != NULL && start == end) {
	      level = (int) atof (result);
	    }
	    else {
	      fprintf (stderr, "\nError in number following compression specification\n");
	      usage();
	    }
	    if (result != NULL) {
	      RELEASE (result);
	    }
	    pop_arg();
	  }
	  if (level >= 0 && level <= 9) {
	    pdf_obj_set_compression(level);
	  } else {
	    fprintf (stderr, "\nNumber following compression specification is out of range\n\n");
	  }
	}
	break;
      default:
	usage();
      }
    }
    argc -= 1 ;
    argv += 1;
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
  if (dvi_filename == NULL) {
    fprintf (stderr, "No dvi filename specified\n");
    usage();
  }
  if (argc > 0) {
    fprintf (stderr, "Multiple dvi filenames?\n");
    usage();
  }
}

static void cleanup(void)
{
  RELEASE (dvi_filename);
  RELEASE (pdf_filename);
}

int CDECL main (int argc, char *argv[]) 
{
  int i;
  static int really_quiet = 0;
  if (argc < 2) {
    usage();
    return 1;
  }
#ifdef KPATHSEA
  kpse_set_progname (argv[0]);
#endif
  argv+=1;
  argc-=1;
  do_args (argc, argv);

  /* Check for ".dvi" at end of argument name */
  if (pdf_filename == NULL)
    set_default_pdf_filename();
  
  if (!really_quiet)
    fprintf (stdout, "%s -> %s\n", dvi_filename, pdf_filename);
  dvi_init (dvi_filename, pdf_filename, mag, x_offset, y_offset);
  if (ignore_colors)
    pdf_special_ignore_colors();
  if (landscape_mode)
    dev_set_page_size (paper_height, paper_width);
  else
    dev_set_page_size (paper_width, paper_height);
  if (paper_specified)
    dev_page_height();
  for (i=0; i<dvi_npages(); i++) {
    fprintf (stderr, "[%d", i+1);
    dvi_do_page (i);
    fprintf (stderr, "]");
  }
  dvi_close();
  fprintf (stderr, "\n");
  cleanup();
  return 0;
}

