/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm-initial/dvipdfm/ebb.c,v 1.2 1998/11/26 20:15:11 mwicks Exp $

    This is ebb, a bounding box extraction program.
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
#include <time.h>
#include "pdfobj.h"
#include "jpeg.h"
#include "mem.h"
#include "pdfparse.h"
#include "numbers.h"

#define EBB_PROGRAM "ebb"
#define EBB_VERSION "Version 0.1"

static void usage (void)
{
  fprintf (stderr, "%s, version %s, Copyright (C) 1998 by Mark A. Wicks\n",
	   EBB_PROGRAM, EBB_VERSION);
  fprintf (stderr, "ebb comes with ABSOLUTELY NO WARRANTY.\n");
  fprintf (stderr, "This is free software, and you are welcome to redistribute it\n");
  fprintf (stderr, "under certain conditions.  Details are distributed with the software.\n");
  fprintf (stderr, "\nUsage: [-v] ebb [files]\n");
  fprintf (stderr, "\t-v\t\tVerbose\n");
  exit(1);
}

static verbose = 0;

static void do_time(FILE *file)
{
  time_t current_time;
  struct tm *bd_time;
  time(&current_time);
  bd_time = localtime(&current_time);
  fprintf (file, "%%%%CreationDate: %s\n", asctime (bd_time));
}

static void write_bb (char *filename, int bbllx, int bblly, int bburx,
		      int bbury) 
{
  char *bbfilename;
  FILE *bbfile;
  bbfilename = NEW (strlen (filename)+4, char);
  strcpy (bbfilename, filename);
  strcat (bbfilename, ".bb");
  if ((bbfile = fopen (bbfilename, "w")) == NULL) {
    fprintf (stderr, "Unable to open output file: %s\n", bbfilename);
    return;
  }
  if (verbose) {
    fprintf (stderr, "okay.\n");
    fprintf (stderr, "Writing to %s:  ", bbfilename);
    fprintf (stderr, "Bounding box:  %d %d %d %d\n", bbllx, bblly,
	     bburx, bbury);
  }
  fprintf (bbfile, "%%%%Title: %s\n", filename);
  fprintf (bbfile, "%%%%Creator: %s %s\n", EBB_PROGRAM, EBB_VERSION);
  fprintf (bbfile, "%%%%BoundingBox: %d %d %d %d\n",
	   bbllx, bblly, bburx, bbury);
  do_time(bbfile);
  fclose (bbfile);
  return;
}


#define PIX2PT (72.0/100.0)
void do_jpeg (FILE *file, char *filename)
{
  struct jpeg *jpeg;
  if (verbose) {
    fprintf (stderr, "%s looks like a JPEG file...", filename);
  }
  jpeg = NEW (1, struct jpeg);
  jpeg -> file = file;
  if (!jpeg_headers (jpeg)) {
        fprintf (stderr, "\n%s: Corrupt JPEG file?\n", filename);
    fclose (file);
    release (jpeg);
    return;
  }
  write_bb (filename, 0, 0,
	    ROUND(jpeg -> width * PIX2PT,1.0),
	    ROUND(jpeg -> height * PIX2PT,1.0));
  release (jpeg);
  return;
}

void do_pdf (FILE *file, char *filename)
{
  pdf_obj *trailer, *catalog, *page_tree, *media_box;
  pdf_obj *kids_ref, *kids, *tmp1;;
  if (verbose) {
    fprintf (stderr, "%s looks like a PDF file...", filename);
  }
  if ((trailer = pdf_open (filename)) == NULL) {
    fprintf (stderr, "Corrupt PDF file?\n");
    return;
  };
  if ((catalog = pdf_deref_obj(pdf_lookup_dict (trailer,"Root"))) ==
      NULL) {
    fprintf (stderr, "\nCatalog isn't where I expect it.\n");
    return;
  }
  /* Got catalog, so done with trailer */
  pdf_release_obj (trailer);
  /* Lookup page tree in catalog */
  page_tree = pdf_deref_obj (pdf_lookup_dict (catalog, "Pages"));
  /* Media box can be inherited so start looking for it now */
  media_box = pdf_deref_obj (pdf_lookup_dict (page_tree, "MediaBox"));
  while ((kids_ref = pdf_lookup_dict (page_tree, "Kids")) != NULL) {
    kids = pdf_deref_obj (kids_ref);
    pdf_release_obj (page_tree);
    page_tree = pdf_deref_obj (pdf_get_array(kids, 1));
    pdf_release_obj (kids);
    /* Replace MediaBox if it's here */
    tmp1 = pdf_deref_obj(pdf_lookup_dict (page_tree, "MediaBox"));
    if (tmp1 && media_box)
      pdf_release_obj (media_box);
    if (tmp1) 
      media_box = tmp1;
  }
  /* At this point, we should have the media box for the first page */ 
  {
    pdf_obj *bbllx, *bblly, *bburx, *bbury;
    double width, height;
    if ((bbllx = pdf_get_array (media_box, 1)) == NULL ||
	(bblly = pdf_get_array (media_box, 2)) == NULL ||
	(bburx = pdf_get_array (media_box, 3)) == NULL ||
	(bbury = pdf_get_array (media_box, 4)) == NULL) {
      fprintf (stderr, "Invalid mediabox\n");
    }
    write_bb (filename,
	      (int) pdf_number_value (bbllx), (int) pdf_number_value (bblly),
	      (int) pdf_number_value (bburx), (int) pdf_number_value (bbury));
  }
  pdf_release_obj (media_box);
  pdf_release_obj (page_tree);
  pdf_close();
}

FILE *inputfile;

main (int argc, char *argv[]) 
{
  argc -= 1;
  argv += 1;
  if (argc == 0)
    usage();
  while (argc > 0 && *argv[0] == '-') {
    switch (*(argv[0]+1)) {
    case 'v':
      verbose = 1;
      argc -= 1;
      argv += 1;
      break;
    case 'h':  
      usage();
      argc -= 1;
      argv += 1;
      break;
    default:
      usage();
    }
  }
  for (; argc > 0; argc--, argv++) {
    if ((inputfile = fopen (argv[0], "r")) == NULL)
      continue;
    if (check_for_jpeg (inputfile)) {
      do_jpeg(inputfile, argv[0]);
      fclose (inputfile);
      continue;
    }
    if (check_for_pdf (inputfile)) {
      do_pdf(inputfile, argv[0]);
      fclose (inputfile);
      continue;
    }
    fprintf (stderr, "Can't handle file type for file named %s\n",
	     argv[0]);
  }
}

/* This is basically a stub so that pdf_special and pdf_doc don't have
   to be linked here.  Unfortunately, the get_reference function
   prevents modularization */

pdf_obj *get_reference (char **start, char *end)
{
  return NULL;
}
