/*
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

	
#include "pdflimits.h"
#include "numbers.h"
#include "error.h"
#include "io.h"
#include "mem.h"
#include "kpathsea/tex-file.h"

static tfm_verbose = 0;
static tfm_debug = 0;

/* TFM Record structure
     Multiple TFM's may be read in at once */

struct a_tfm
{
  UNSIGNED_PAIR wlenfile;
  UNSIGNED_PAIR wlenheader;
  UNSIGNED_PAIR bc;
  UNSIGNED_PAIR ec;
  UNSIGNED_PAIR nwidths;
  UNSIGNED_PAIR nheights;
  UNSIGNED_PAIR ndepths;
  UNSIGNED_PAIR nitcor;
  UNSIGNED_PAIR nlig;
  UNSIGNED_PAIR nkern;
  UNSIGNED_PAIR nextens;
  UNSIGNED_PAIR nfonparm;
  SIGNED_QUAD *header;
  UNSIGNED_QUAD *char_info;
  SIGNED_QUAD *width;
  SIGNED_QUAD *height;
  SIGNED_QUAD *depth;
  SIGNED_QUAD *italic;
  SIGNED_QUAD *lig_kern;
  SIGNED_QUAD *kern;
  SIGNED_QUAD *exten;
  SIGNED_QUAD *param;
};

struct a_tfm tfm[MAX_FONTS];
static numtfms = 0; /* numtfms should equal numfonts in dvi.c */

static invalid_tfm_file (void)
{
  ERROR ("tfm_open: Something is wrong.  Are you sure this is a valid TFM file?\n");
  
}

/* External Routine */

void tfm_set_verbose (void)
{
  tfm_verbose = 1;
}

void tfm_set_debug (void)
{
  tfm_verbose = 1;
  tfm_debug = 1;
}


FILE *tfm_file;
unsigned tfm_file_size;

static sum_of_tfm_sizes (struct a_tfm *a_tfm)
{
  unsigned long result = 6;
  result += (a_tfm -> ec - a_tfm -> bc + 1);
  result += a_tfm -> wlenheader;
  result += a_tfm -> nwidths;
  result += a_tfm -> nheights;
  result += a_tfm -> ndepths;
  result += a_tfm -> nitcor;
  result += a_tfm -> nlig;
  result += a_tfm -> nkern;
  result += a_tfm -> nextens;
  result += a_tfm -> nfonparm;
  return result;
}


static get_sizes (struct a_tfm *a_tfm)
{
  a_tfm -> wlenfile = get_unsigned_pair (tfm_file);
  a_tfm -> wlenheader = get_unsigned_pair (tfm_file);
  a_tfm -> bc = get_unsigned_pair (tfm_file);
  a_tfm -> ec = get_unsigned_pair (tfm_file);
  if (a_tfm -> ec < a_tfm -> bc)
    invalid_tfm_file();
  a_tfm -> nwidths = get_unsigned_pair (tfm_file);
  a_tfm -> nheights = get_unsigned_pair (tfm_file);
  a_tfm -> ndepths = get_unsigned_pair (tfm_file);
  a_tfm -> nitcor = get_unsigned_pair (tfm_file);
  a_tfm -> nlig = get_unsigned_pair (tfm_file);
  a_tfm -> nkern = get_unsigned_pair (tfm_file);
  a_tfm -> nextens = get_unsigned_pair (tfm_file);
  a_tfm -> nfonparm = get_unsigned_pair (tfm_file);
  if (4 * (a_tfm -> wlenfile) != tfm_file_size ||
      sum_of_tfm_sizes (a_tfm) != a_tfm -> wlenfile)
    invalid_tfm_file();
  if (tfm_debug) {
    fprintf (stderr, "Computed size (words)%d\n", sum_of_tfm_sizes (a_tfm));
    fprintf (stderr, "Stated size (words)%d\n", a_tfm -> wlenfile);
    fprintf (stderr, "Actual size (bytes)%d\n", tfm_file_size);
  }
}

static dump_sizes (struct a_tfm *a_tfm)
{
  fprintf (stderr, "wlenfile: %d\n", a_tfm -> wlenfile);
  fprintf (stderr, "wlenheader: %d\n", a_tfm -> wlenheader);
  fprintf (stderr, "bc: %d\n", a_tfm -> bc);
  fprintf (stderr, "ec: %d\n", a_tfm -> ec);
  fprintf (stderr, "nwidths: %d\n", a_tfm -> nwidths);
  fprintf (stderr, "nheights: %d\n", a_tfm -> nheights);
  fprintf (stderr, "ndepths: %d\n", a_tfm -> ndepths);
  fprintf (stderr, "nitcor: %d\n", a_tfm -> nitcor);
  fprintf (stderr, "nlig: %d\n", a_tfm -> nlig);
  fprintf (stderr, "nkern: %d\n", a_tfm -> nkern);
  fprintf (stderr, "nextens: %d\n", a_tfm -> nextens);
  fprintf (stderr, "nfonparm: %d\n", a_tfm -> nfonparm);
}


static get_fix_word_array (SIGNED_QUAD *a_word, int length)
{
  unsigned i;
  for (i=0; i< length; i++) {
    a_word[i] = get_signed_quad (tfm_file);
  }
}

static get_unsigned_quad_array (UNSIGNED_QUAD *a_word, int length)
{
  unsigned i;
  for (i=0; i< length; i++) {
    a_word[i] = get_unsigned_quad (tfm_file);
  }
}

static do_fix_word_array (SIGNED_QUAD **a, UNSIGNED_PAIR len)
{
  *a = NEW (len, SIGNED_QUAD);
  get_fix_word_array (*a, len);
}

static do_unsigned_quad_array (UNSIGNED_QUAD **a, UNSIGNED_PAIR len)
{
  *a = NEW (len, UNSIGNED_QUAD);
  get_unsigned_quad_array (*a, len);
}

static get_arrays (struct a_tfm *a_tfm)
{
  if (tfm_debug) fprintf (stderr, "Reading %d word header\n",
			  a_tfm->wlenheader);
  do_fix_word_array (&(a_tfm -> header), a_tfm -> wlenheader);
  if (tfm_debug) fprintf (stderr, "Reading %d char_infos\n",
			  (a_tfm->ec)-(a_tfm->bc)+1);
  do_unsigned_quad_array (&(a_tfm -> char_info), (a_tfm->ec)-(a_tfm->bc)+1);
  if (tfm_debug) fprintf (stderr, "Reading %d widths\n",
			  a_tfm -> nwidths);
  do_fix_word_array (&(a_tfm -> width), a_tfm -> nwidths);
  if (tfm_debug) fprintf (stderr, "Reading %d heights\n",
			  a_tfm -> nheights);
  do_fix_word_array (&(a_tfm -> height), a_tfm -> nheights);
  if (tfm_debug) fprintf (stderr, "Reading %d depths\n",
			  a_tfm -> ndepths);
  do_fix_word_array (&(a_tfm -> depth), a_tfm -> ndepths);
  if (tfm_debug) fprintf (stderr, "Reading %d italic corrections\n",
			  a_tfm -> nitcor);
  do_fix_word_array (&(a_tfm -> italic), a_tfm -> nitcor);
  if (tfm_debug) fprintf (stderr, "Reading %d ligatures\n",
			  a_tfm -> nlig);
  do_fix_word_array (&(a_tfm -> lig_kern), a_tfm -> nlig);
  if (tfm_debug) fprintf (stderr, "Reading %d kerns\n",
			  a_tfm -> nkern);
  do_fix_word_array (&(a_tfm -> kern), a_tfm -> nkern);
  if (tfm_debug) fprintf (stderr, "Reading %d extens\n",
			  a_tfm -> nextens);
  do_fix_word_array (&(a_tfm -> exten), a_tfm -> nextens);
  if (tfm_debug) fprintf (stderr, "Reading %d fontparms\n",
			  a_tfm -> nfonparm);
  do_fix_word_array (&(a_tfm -> param), a_tfm -> nfonparm);
}

static get_tfm (struct a_tfm *a_tfm)
{
  get_sizes (a_tfm);
  get_arrays (a_tfm);
}

/* External Routine */


int tfm_open (char *tex_font_name)
{
  char *tfm_file_name, *full_tfm_file_name;
  tfm_file_name = NEW (strlen(tex_font_name)+5, char);
  strcpy (tfm_file_name, tex_font_name);
  strcat (tfm_file_name, ".tfm");
  full_tfm_file_name = kpse_find_tfm (tfm_file_name);
  if (full_tfm_file_name == NULL) {
    fprintf (stderr, "%s: ", tfm_file_name);
    ERROR ("tfm_open:  Unable to find TFM file");
  }
  if (numtfms >= MAX_FONTS) {
    ERROR ("tfm_open:  Tried to open too many TFM files!");
  }
  fprintf (stderr, "(%s", tfm_file_name);
  release (tfm_file_name);
  if (!(tfm_file = fopen (full_tfm_file_name, "r"))) {
    fprintf (stderr, "tfm_open: %s\n", tfm_file_name);
    ERROR ("tfm_open:  Specified TFM file cannot be opened");
  }
  if ((tfm_file_size = file_size(tfm_file)) < 24) {
    invalid_tfm_file ();
  }
  get_tfm (&tfm[numtfms]);
  fclose (tfm_file);
  fprintf (stderr, ")");
  if (tfm_verbose) {
    dump_sizes (&tfm[numtfms]);
  }
  return numtfms++;
}

/* tfm_get_width returns the width of the font
   as a fraction of the design size */

double tfm_get_width (int font_id, UNSIGNED_PAIR ch)
{
  UNSIGNED_QUAD charinfo;
  UNSIGNED_PAIR width_index;
  if (ch < tfm[font_id].bc || ch > tfm[font_id].ec)
    ERROR ("tfm_get_width: Character not in tfm file");
  charinfo = (tfm[font_id].char_info)[ch-tfm[font_id].bc];
  width_index = (charinfo / 16777216ul);
  return (double) (tfm[font_id].width)[width_index] / 1048576.0;
}

UNSIGNED_PAIR tfm_get_firstchar (int font_id)
{
  return tfm[font_id].bc;
}

UNSIGNED_PAIR tfm_get_lastchar (int font_id)
{
  return tfm[font_id].ec;
}












