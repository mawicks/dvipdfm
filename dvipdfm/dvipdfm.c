/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/dvipdfm.c,v 1.45 1999/08/21 02:47:56 mwicks Exp $

    This is dvipdfm, a DVI to PDF translator.
    Copyright (C) 1998, 1999 by Mark A. Wicks

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
#include <limits.h>
#include "config.h"
#include <ctype.h>
#include "system.h"
#include "dvi.h"
#include "mem.h"
#include "pdfdoc.h"
#include "pdfdev.h"
#include "type1.h"
#include "pdfspecial.h"
#include "pdfparse.h"
#include "vf.h"
#include "pkfont.h"
#include "thumbnail.h"

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
   fprintf (stdout, "%s, version %s, Copyright (C) 1998, 1999 by Mark A. Wicks\n", PACKAGE, VERSION);
   fprintf (stdout, "dvipdfm comes with ABSOLUTELY NO WARRANTY.\n");
   fprintf (stdout, "This is free software, and you are welcome to redistribute it\n");
   fprintf (stdout, "under certain conditions.  Details are distributed with the software.\n");
   fprintf (stdout, "\nUsage: dvipdfm [options] dvifile\n");
   fprintf (stdout, "-c      \tIgnore color specials (for B&W printing)\n");
   fprintf (stdout, "-f filename\tSet font map file name [t1fonts.map]\n");
   fprintf (stdout, "-o filename\tSet output file name [dvifile.pdf]\n");
   fprintf (stdout, "-l \t\tLandscape mode\n");
   fprintf (stdout, "-m number\tSet additional magnification\n");
   fprintf (stdout, "-p papersize\tSet papersize (letter, legal,\n");
   fprintf (stdout, "            \tledger, tabloid, a4, or a3) [letter]\n");
   fprintf (stdout, "-r resolution\tSet resolution (in DPI) for raster fonts [600]\n");
   fprintf (stdout, "-s pages\tSelect page ranges (-)\n");
   fprintf (stdout, "-t      \tEmbed thumbnail images\n");
   fprintf (stdout, "-d      \tRemove thumbnail images when finished\n");
   fprintf (stdout, "-x dimension\tSet horizontal offset [1.0in]\n");
   fprintf (stdout, "-y dimension\tSet vertical offset [1.0in]\n");
   fprintf (stdout, "-e          \tDisable partial font embedding [default is enabled])\n");
   fprintf (stdout, "-z number\tSet compression level (0-9) [default is 9])\n");
   fprintf (stdout, "-v          \tBe verbose\n");
   fprintf (stdout, "-vv         \tBe more verbose\n");
   fprintf (stdout, "\nAll dimensions entered on the command line are \"true\" TeX dimensions.\n");
   fprintf (stdout, "Argument of \"-s\" lists physical page ranges separated by commas, e.g., \"-s 1-3,5-6\"\n\n");
   exit(1);
}

static double paper_width = 612.0, paper_height = 792.0;
static int paper_specified = 0;
static landscape_mode = 0;
static ignore_colors = 0;
static double mag = 1.0, x_offset=72.0, y_offset=72.0;
static int font_dpi = 600;

struct page_range 
{
  unsigned int first, last;
} *page_ranges = NULL;
int max_page_ranges = 0, num_page_ranges = 0;

#define pop_arg() {argv += 1; argc -= 1;}

static void do_args (int argc, char *argv[])
{
  char *flag;
  while (argc > 0 && *argv[0] == '-') {
    for (flag=argv[0]+1; *flag != 0; flag++) {
      switch (*flag) {
      case 'r':
	if (argc < 2) {
	  fprintf (stderr, "\nResolution specification missing a number\n\n");
	  usage();
	}
	{
	  char *result, *end, *start = argv[1];
	  end = start + strlen(argv[1]);
	  result = parse_number (&start, end);
	  if (result != NULL && start == end) {
	    font_dpi = (int) atof (result);
	  }
	  else {
	    fprintf (stderr, "\nError in number following resolution specification\n\n");
	    usage();
	  }
	  if (result != NULL) {
	    RELEASE (result);
	  }
	}
	pop_arg();
	break;
      case 'm':
	if (argc < 2) {
	  fprintf (stderr, "\nMagnification specification missing a number\n\n");
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
	    fprintf (stderr, "\nError in number following magnification specification\n\n");
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
	  fprintf (stderr, "\nX Offset specification missing a number\n\n");
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
	    fprintf (stderr, "\nError in number following xoffset specification\n\n");
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
	    fprintf (stderr, "\nError in dimension specification following xoffset\n\n");
	    usage();
	  }
	}
	pop_arg();
	break;
      case 'y':
	if (argc < 2) {
	  fprintf (stderr, "\nY offset specification missing a number\n\n");
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
	    fprintf (stderr, "\nError in number following yoffset specification\n\n");
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
	    fprintf (stderr, "\nError in dimension specification following yoffset\n\n");
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
      case 's':
	{
	  char *result, *end, *start = argv[1];
	  if (argc < 2)
	    ERROR ("Missing page selection specification");
	  end = start + strlen (argv[1]);
	  while (start < end) {
	    /* Enlarge page range table if necessary */
	    if (num_page_ranges >= max_page_ranges) {
	      max_page_ranges += 4;
	      page_ranges = RENEW (page_ranges, max_page_ranges,
				   struct page_range);
	    }
	    skip_white (&start, end);
	    page_ranges[num_page_ranges].first = 0;
	    if ((result = parse_unsigned (&start, end))) {
	      page_ranges[num_page_ranges].first = atoi(result)-1;
	      page_ranges[num_page_ranges].last =
		page_ranges[num_page_ranges].first;
	      RELEASE(result);
	    }
	    skip_white (&start, end);
	    if (*start == '-') {
	      start += 1;
	      page_ranges[num_page_ranges].last = UINT_MAX;
	      skip_white (&start, end);
	      if (start < end &&
		  ((result = parse_unsigned (&start, end)))) {
		page_ranges[num_page_ranges].last = atoi(result)-1;
		RELEASE (result);
	      }
	    }
	    num_page_ranges += 1;
	    skip_white (&start, end);
	    if (start < end && *start == ',') {
	      start += 1;
	      continue;
	    }
	    skip_white (&start, end);
	    if (start < end) {
	      fprintf (stderr, "Page selection? %s", start);
	      ERROR ("Bad page range specification");
	    }
	  }
	  pop_arg();
	}
	break;
      case 't':
	{
#ifdef HAVE_LIBPNG
	  pdf_doc_enable_thumbnails ();
#else
	  ERROR ("The thumbnail option requires libpng, which you apparently don't have");
#endif	   
	}
	break;
      case 'd':
	{
#ifdef HAVE_LIBPNG
	  thumb_remove ();
#else
	  ERROR ("The thumbnail option requires libpng, which you apparently don't have");
#endif	   
	}
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
	pk_set_verbose();
	pdf_obj_set_verbose();
	pdf_doc_set_verbose();
	break;
      case 'z': 
	{
	  int level = 9;
#ifndef HAVE_ZLIB
	  fprintf (stderr, "\nYou don't have compression compiled in.  Possibly libz wasn't found by configure.\nCompression specification will be ignored.\n\n");
#endif  /* HAVE_ZLIB */
	  if (isdigit (*(flag+1))) {
	    level = *(++flag) - '0';
	  } else {
	    char *result, *end, *start = argv[1];
	    if (argc < 2) {
	      fprintf (stderr, "\nCompression specification missing number for level\n\n");
	      usage();
	    }
	    end = start + strlen(argv[1]);
	    result = parse_number (&start, end);
	    if (result != NULL && start == end) {
	      level = (int) atof (result);
	    }
	    else {
	      fprintf (stderr, "\nError in number following compression specification\n\n");
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
  if (argc < 1) {
    fprintf (stderr, "\nNo dvi filename specified.\n\n");
    usage();
  }
  if (argc > 1) {
    fprintf (stderr, "\nMultiple dvi filenames?\n\n");
    usage();
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
}

static void cleanup(void)
{
  RELEASE (dvi_filename);
  RELEASE (pdf_filename);
  if (page_ranges)
    RELEASE (page_ranges);
}

int CDECL main (int argc, char *argv[]) 
{
  int i;
  static int really_quiet = 0;
  int at_least_one_page = 0;
  if (argc < 2) {
    usage();
    return 1;
  }
#ifdef KPATHSEA
  kpse_set_program_name (argv[0], NULL);
#endif
  argv+=1;
  argc-=1;
  do_args (argc, argv);

#ifdef KPATHSEA
  kpse_init_prog ("", font_dpi, NULL, "cmr10");
  pk_set_dpi (font_dpi);
  kpse_set_program_enabled (kpse_pk_format, 1, kpse_src_texmf_cnf);
#endif

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
  if ((num_page_ranges))
    for (i=0; i<num_page_ranges; i++) {
      unsigned j;
      if (page_ranges[i].first <= page_ranges[i].last)
	for (j=page_ranges[i].first; j<=page_ranges[i].last && j<dvi_npages(); j++) {
	  fprintf (stderr, "[%d", j+1);
	  dvi_do_page (j);
	  at_least_one_page = 1;
	  fprintf (stderr, "]");
	}
      else
	for (j=page_ranges[i].first;
	     j>=page_ranges[i].last &&
	       j<dvi_npages() &&
	       j >= 0; j--) {
	  fprintf (stderr, "[%d", j+1);
	  dvi_do_page (j);
	  at_least_one_page = 1;
	  fprintf (stderr, "]");
	}
    }
  if (!at_least_one_page && num_page_ranges) {
    fprintf (stderr, "No pages fall in range!\nFalling back to entire document.\n");
  }
  if (!at_least_one_page) /* Fall back to entire document */
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

