/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/tfm.c,v 1.28.8.1 2000/07/24 23:37:27 mwicks Exp $

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
#include "system.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"
#include "pdflimits.h"
#include "numbers.h"

#define FWBASE ((double) (1<<20))

static char tfm_verbose = 0;
static char tfm_debug = 0;

/* TFM Record structure
     Multiple TFM's may be read in at once */

struct a_tfm
{
  UNSIGNED_QUAD wlenfile;
  UNSIGNED_QUAD wlenheader;
  UNSIGNED_QUAD bc;
  UNSIGNED_QUAD ec;
  UNSIGNED_QUAD nwidths;
  UNSIGNED_QUAD nheights;
  UNSIGNED_QUAD ndepths;
  UNSIGNED_QUAD nitcor;
  UNSIGNED_QUAD nlig;
  UNSIGNED_QUAD nkern;
  UNSIGNED_QUAD nextens;
  UNSIGNED_QUAD nfonparm;
  UNSIGNED_QUAD font_direction;	/* Used only in OFMs.  TFMs don't have
				 this field*/
  SIGNED_QUAD *header;
  UNSIGNED_QUAD *char_info;
  SIGNED_QUAD *width;
  SIGNED_QUAD *height;
  SIGNED_QUAD *depth;
  char *tex_name;
  fixword *unpacked_widths;
  fixword *unpacked_heights;
  fixword *unpacked_depths;
};

struct a_tfm *tfm = NULL;
static unsigned numtfms = 0, max_tfms = 0; /* numtfms should equal
					      numfonts in dvi.c */
static void need_more_tfms (unsigned n)
{
  if (numtfms + n > max_tfms) {
    max_tfms += MAX_FONTS;
    tfm = RENEW (tfm, max_tfms, struct a_tfm);
  }
}

static void invalid_tfm_file (void)
{
  ERROR ("Something is wrong.  Are you sure this is a valid TFM file?\n");
}

static void invalid_ofm_file (void)
{
  ERROR ("Something is wrong.  Are you sure this is a valid OFM file?\n");
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

static UNSIGNED_PAIR sum_of_tfm_sizes (struct a_tfm *a_tfm)
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

static SIGNED_QUAD sum_of_ofm_sizes (struct a_tfm *a_tfm)
{
  unsigned long result = 14;
  result += 2*(a_tfm -> ec - a_tfm -> bc + 1);
  result += a_tfm -> wlenheader;
  result += a_tfm -> nwidths;
  result += a_tfm -> nheights;
  result += a_tfm -> ndepths;
  result += a_tfm -> nitcor;
  result += 2*(a_tfm -> nlig);
  result += a_tfm -> nkern;
  result += 2*(a_tfm -> nextens);
  result += a_tfm -> nfonparm;
  return result;
}


static void get_sizes (FILE *tfm_file, SIGNED_QUAD tfm_file_size,
		       struct a_tfm *a_tfm)
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
  if ( a_tfm -> wlenfile != tfm_file_size/4 ||
      sum_of_tfm_sizes (a_tfm) != a_tfm -> wlenfile)
    invalid_tfm_file();
  if (tfm_debug) {
    fprintf (stderr, "Computed size (words)%d\n", sum_of_tfm_sizes (a_tfm));
    fprintf (stderr, "Stated size (words)%ld\n", a_tfm -> wlenfile);
    fprintf (stderr, "Actual size (bytes)%ld\n", tfm_file_size);
  }
  return;
}

static void ofm_get_sizes (FILE *ofm_file,  UNSIGNED_QUAD ofm_file_size,
			   struct a_tfm *a_tfm)
{
  SIGNED_QUAD level;
  if ((level = get_signed_quad(ofm_file)) != 0) {
    ERROR ("OFM Level > 0.  Can't handle this file");
  }
  a_tfm -> wlenfile = get_signed_quad (ofm_file);
  a_tfm -> wlenheader = get_signed_quad (ofm_file);
  a_tfm -> bc = get_signed_quad (ofm_file);
  a_tfm -> ec = get_signed_quad (ofm_file);
  if (a_tfm -> ec < a_tfm -> bc)
    invalid_ofm_file();
  a_tfm -> nwidths = get_signed_quad (ofm_file);
  a_tfm -> nheights = get_signed_quad (ofm_file);
  a_tfm -> ndepths = get_signed_quad (ofm_file);
  a_tfm -> nitcor = get_signed_quad (ofm_file);
  a_tfm -> nlig = get_signed_quad (ofm_file);
  a_tfm -> nkern = get_signed_quad (ofm_file);
  a_tfm -> nextens = get_signed_quad (ofm_file);
  a_tfm -> nfonparm = get_signed_quad (ofm_file);
  a_tfm -> font_direction = get_signed_quad (ofm_file);
  if ( a_tfm -> wlenfile != ofm_file_size/4 ||
      sum_of_ofm_sizes (a_tfm) != a_tfm -> wlenfile)
    invalid_ofm_file();
  if (tfm_debug) {
    fprintf (stderr, "Computed size (words)%ld\n", sum_of_ofm_sizes (a_tfm));
    fprintf (stderr, "Stated size (words)%ld\n", a_tfm -> wlenfile);
    fprintf (stderr, "Actual size (bytes)%ld\n", ofm_file_size);
  }
  return;
}

static void dump_sizes (struct a_tfm *a_tfm)
{
  fprintf (stderr, "wlenfile: %ld\n", a_tfm -> wlenfile);
  fprintf (stderr, "wlenheader: %ld\n", a_tfm -> wlenheader);
  fprintf (stderr, "bc: %ld\n", a_tfm -> bc);
  fprintf (stderr, "ec: %ld\n", a_tfm -> ec);
  fprintf (stderr, "nwidths: %ld\n", a_tfm -> nwidths);
  fprintf (stderr, "nheights: %ld\n", a_tfm -> nheights);
  fprintf (stderr, "ndepths: %ld\n", a_tfm -> ndepths);
  fprintf (stderr, "nitcor: %ld\n", a_tfm -> nitcor);
  fprintf (stderr, "nlig: %ld\n", a_tfm -> nlig);
  fprintf (stderr, "nkern: %ld\n", a_tfm -> nkern);
  fprintf (stderr, "nextens: %ld\n", a_tfm -> nextens);
  fprintf (stderr, "nfonparm: %ld\n", a_tfm -> nfonparm);
  return;
}


static void get_fix_word_array (FILE *tfm_file, SIGNED_QUAD *a_word,
				SIGNED_QUAD length)
{
  unsigned i;
  for (i=0; i< length; i++) {
    a_word[i] = get_signed_quad (tfm_file);
  }
  return;
}

static void get_unsigned_quad_array (FILE *tfm_file, UNSIGNED_QUAD *a_word,
				     SIGNED_QUAD length)
{
  unsigned i;
  for (i=0; i< length; i++) {
    a_word[i] = get_unsigned_quad (tfm_file);
  }
  return;
}

static void do_fix_word_array (FILE *tfm_file, SIGNED_QUAD **a, SIGNED_QUAD len)
{
  if (len != 0) {
    *a = NEW (len, SIGNED_QUAD);
    get_fix_word_array (tfm_file, *a, len);
  } else
    *a = NULL;
  return;
}

static void do_unsigned_quad_array (FILE *tfm_file, UNSIGNED_QUAD **a, UNSIGNED_PAIR len)
{
  if (len != 0) {
    *a = NEW (len, UNSIGNED_QUAD);
    get_unsigned_quad_array (tfm_file, *a, len);
  } else
    *a = NULL;
  return;
}
static void unpack_widths(struct a_tfm *a_tfm)
{
  int i;
  UNSIGNED_QUAD charinfo;
  UNSIGNED_PAIR width_index;
  a_tfm -> unpacked_widths = NEW (256, fixword);
  for (i=0; i<256; i++) {
    (a_tfm ->unpacked_widths)[i] = 0;
  }
  for (i=(a_tfm->bc); i<=(a_tfm->ec); i++ ) {
    charinfo = (a_tfm->char_info)[i-(a_tfm->bc)];
    width_index = (charinfo / 16777216ul);
    (a_tfm->unpacked_widths)[i] = (a_tfm->width)[width_index];
  }
  return;
}

static void unpack_heights(struct a_tfm *a_tfm)
{
  int i;
  UNSIGNED_QUAD charinfo;
  UNSIGNED_PAIR height_index;
  a_tfm -> unpacked_heights = NEW (256, fixword);
  for (i=0; i<256; i++) {
    (a_tfm ->unpacked_heights)[i] = 0;
  }
  for (i=(a_tfm->bc); i<=(a_tfm->ec); i++ ) {
    charinfo = (a_tfm->char_info)[i-(a_tfm->bc)];
    height_index = (charinfo / 0x100000ul) & 0xf;
    (a_tfm->unpacked_heights)[i] = (a_tfm->height)[height_index];
  }
  return;
}

static void unpack_depths(struct a_tfm *a_tfm)
{
  int i;
  UNSIGNED_QUAD charinfo;
  UNSIGNED_PAIR depth_index;
  a_tfm -> unpacked_depths = NEW (256, fixword);
  for (i=0; i<256; i++) {
    (a_tfm ->unpacked_depths)[i] = 0;
  }
  for (i=(a_tfm->bc); i<=(a_tfm->ec); i++ ) {
    charinfo = (a_tfm->char_info)[i-(a_tfm->bc)];
    depth_index = (charinfo / 0x10000ul) & 0xf;
    (a_tfm->unpacked_depths)[i] = (a_tfm->depth)[depth_index];
  }
  return;
}

static void get_arrays (FILE *tfm_file, struct a_tfm *a_tfm)
{
  if (tfm_debug) fprintf (stderr, "Reading %ld word header\n",
			  a_tfm->wlenheader);
  do_fix_word_array (tfm_file, &(a_tfm -> header), a_tfm -> wlenheader);
  if (tfm_debug) fprintf (stderr, "Reading %ld char_infos\n",
			  (a_tfm->ec)-(a_tfm->bc)+1);
  do_unsigned_quad_array (tfm_file, &(a_tfm -> char_info), (a_tfm->ec)-(a_tfm->bc)+1);
  if (tfm_debug) fprintf (stderr, "Reading %ld widths\n",
			  a_tfm -> nwidths);
  do_fix_word_array (tfm_file, &(a_tfm -> width), a_tfm -> nwidths);
  if (tfm_debug) fprintf (stderr, "Reading %ld heights\n",
			  a_tfm -> nheights);
  do_fix_word_array (tfm_file, &(a_tfm -> height), a_tfm -> nheights);
  if (tfm_debug) fprintf (stderr, "Reading %ld depths\n",
			  a_tfm -> ndepths);
  do_fix_word_array (tfm_file, &(a_tfm -> depth), a_tfm -> ndepths);
  unpack_widths (a_tfm);
  unpack_heights (a_tfm);
  unpack_depths (a_tfm);
  return;
}

static void do_ofm_char_info (FILE *tfm_file, struct a_tfm *a_tfm,
			      UNSIGNED_QUAD num_chars)
{
  unsigned i;
  if (num_chars != 0) {
    a_tfm -> unpacked_widths = NEW (num_chars, fixword);
    a_tfm -> unpacked_heights = NEW (num_chars, fixword);
    a_tfm -> unpacked_depths = NEW (num_chars, fixword);
  }
  for (i=0; i<num_chars; i++) {
    UNSIGNED_PAIR width;
    UNSIGNED_BYTE height;
    UNSIGNED_BYTE depth;
    width = get_unsigned_pair (tfm_file);
    height = get_unsigned_byte (tfm_file);
    depth = get_unsigned_byte (tfm_file);
    /* Ignore remaining quad */
    get_unsigned_quad (tfm_file);
    (a_tfm->unpacked_widths)[i] = (a_tfm->width)[width];
    (a_tfm->unpacked_heights)[i] = (a_tfm->height)[height];
    (a_tfm->unpacked_depths)[i] = (a_tfm->depth)[depth];
  }
}


static void ofm_get_arrays (FILE *tfm_file, struct a_tfm *a_tfm)
{
  if (tfm_debug) fprintf (stderr, "Reading %ld word header\n",
			  a_tfm->wlenheader);
  do_fix_word_array (tfm_file, &(a_tfm -> header), a_tfm -> wlenheader);
  if (tfm_debug) fprintf (stderr, "Reading %ld char_infos\n",
			  (a_tfm->ec)-(a_tfm->bc)+1);
  do_ofm_char_info (tfm_file, a_tfm, (a_tfm->ec)-(a_tfm->bc)+1);
  if (tfm_debug) fprintf (stderr, "Reading %ld widths\n",
			  a_tfm -> nwidths);
  do_fix_word_array (tfm_file, &(a_tfm -> width), a_tfm -> nwidths);
  if (tfm_debug) fprintf (stderr, "Reading %ld heights\n",
			  a_tfm -> nheights);
  do_fix_word_array (tfm_file, &(a_tfm -> height), a_tfm -> nheights);
  if (tfm_debug) fprintf (stderr, "Reading %ld depths\n",
			  a_tfm -> ndepths);
  do_fix_word_array (tfm_file, &(a_tfm -> depth), a_tfm -> ndepths);
  return;
}



static void get_ofm (FILE *ofm_file, UNSIGNED_QUAD ofm_file_size,
		     struct a_tfm *a_tfm)
{
  ofm_get_sizes (ofm_file, ofm_file_size, a_tfm);
  ofm_get_arrays (ofm_file, a_tfm);
  return;
}

static void get_tfm (FILE *tfm_file, UNSIGNED_QUAD tfm_file_size,
		     struct a_tfm *a_tfm)
{
  get_sizes (tfm_file, tfm_file_size, a_tfm);
  get_arrays (tfm_file, a_tfm);
  return;
}

/* External Routine */

int tfm_open (const char *tfm_name)
{
  FILE *tfm_file;
  int i;
  UNSIGNED_QUAD tfm_file_size;
  char *full_tfm_file_name;
  for (i=0; i<numtfms; i++) {
    if (!strcmp (tfm_name, tfm[i].tex_name))
      break;
  }
  if (i == numtfms) { /* Name hasn't already been loaded */
    full_tfm_file_name = kpse_find_tfm (tfm_name);
    if (full_tfm_file_name == NULL) {
      fprintf (stderr, "\n%s: ", tfm_name);
      ERROR ("tfm_open:  Unable to find TFM file");
    }
    need_more_tfms (1);
    if (!(tfm_file = FOPEN (full_tfm_file_name, FOPEN_RBIN_MODE))) {
      fprintf (stderr, "tfm_open: %s\n", tfm_name);
    ERROR ("tfm_open:  Specified TFM file cannot be opened");
    }
    if ((tfm_file_size = file_size(tfm_file)) < 24) {
      invalid_tfm_file ();
    }
    tfm[numtfms].tex_name = NEW (strlen(tfm_name)+1, char);
    strcpy (tfm[numtfms].tex_name, tfm_name);
    get_tfm (tfm_file, tfm_file_size, &tfm[numtfms]);
    FCLOSE (tfm_file);
    if (tfm_verbose) {
      dump_sizes (&tfm[numtfms]);
    }
    return numtfms++;
  } else { /* Name has been loaded before */
    return i;
  }
}

void tfm_close_all(void)
{
  int i;
  for (i=0; i<numtfms; i++) {
    if (tfm[i].header)
      RELEASE (tfm[i].header);
    if (tfm[i].char_info)
      RELEASE (tfm[i].char_info);
    if (tfm[i].width)
      RELEASE (tfm[i].width);
    if (tfm[i].height)
      RELEASE (tfm[i].height);
    if (tfm[i].depth)
      RELEASE (tfm[i].depth);
    RELEASE (tfm[i].tex_name);
    RELEASE (tfm[i].unpacked_widths);
    RELEASE (tfm[i].unpacked_heights);
    RELEASE (tfm[i].unpacked_depths);
  }
  if (tfm)
    RELEASE (tfm);
}

/* tfm_get_width returns the width of the font
   as a (double) fraction of the design size */
double tfm_get_width (int font_id, UNSIGNED_BYTE ch)
{
  if (tfm[font_id].unpacked_widths)
    return (double) (tfm[font_id].unpacked_widths)[ch] / FWBASE;
  else return 0.0;
}

double tfm_get_height (int font_id, UNSIGNED_BYTE ch)
{
  if (tfm[font_id].unpacked_heights)
    return (double) (tfm[font_id].unpacked_heights)[ch] / FWBASE;
  else return 0.0;
}

double tfm_get_depth (int font_id, UNSIGNED_BYTE ch)
{
  if (tfm[font_id].unpacked_depths)
    return (tfm[font_id].unpacked_depths)[ch]/FWBASE;
  else return 0.0;
}

fixword tfm_get_fw_width (int font_id, UNSIGNED_BYTE ch)
{
  if (tfm[font_id].unpacked_widths)
    return (tfm[font_id].unpacked_widths)[ch];
  return 0;
}

fixword tfm_get_fw_height (int font_id, UNSIGNED_BYTE ch)
{
  if (tfm[font_id].unpacked_heights)
    return (tfm[font_id].unpacked_heights)[ch];
  return 0;
}

fixword tfm_get_fw_depth (int font_id, UNSIGNED_BYTE ch)
{
  if (tfm[font_id].unpacked_depths)
    return (tfm[font_id].unpacked_depths)[ch];
  return 0;
}

fixword tfm_string_width (int font_id, unsigned char *s, unsigned len)
{
  fixword result = 0;
  unsigned i;
  if (tfm[font_id].unpacked_widths) 
    for (i=0; i<len; i++) {
      result += tfm[font_id].unpacked_widths[s[i]];
    }
  return result;
}

fixword tfm_string_depth (int font_id, unsigned char *s, unsigned len)
{
  fixword result = 0;
  unsigned i;
  if (tfm[font_id].unpacked_depths) 
    for (i=0; i<len; i++) {
      result = MAX(result, tfm[font_id].unpacked_depths[s[i]]);
    }
  return result;
}

fixword tfm_string_height (int font_id, unsigned char *s, unsigned len)
{
  fixword result = 0;
  unsigned i;
  if (tfm[font_id].unpacked_heights) 
    for (i=0; i<len; i++) {
      result = MAX(result, tfm[font_id].unpacked_heights[s[i]]);
    }
  return result;
}

UNSIGNED_PAIR tfm_get_firstchar (int font_id)
{
  return tfm[font_id].bc;
}

UNSIGNED_PAIR tfm_get_lastchar (int font_id)
{
  return tfm[font_id].ec;
}

double tfm_get_design_size (int font_id)
{
  return ((tfm[font_id].header))[1]/FWBASE*(72.0/72.27);
}


double tfm_get_max_width (int font_id)
{
  SIGNED_QUAD max = 0;
  int i;
  for (i=0; i<tfm[font_id].nwidths; i++) {
    if ((tfm[font_id].width)[i] > max)
      max = (tfm[font_id].width)[i];
  }
  return (max/FWBASE);
}

int tfm_is_fixed_width (int font_id)
{
  /* We always have two widths since width[0] = 0.
     A fixed width font will have width[1] = something
     and not have any other widths */
  return (tfm[font_id].nwidths == 2);
}

double tfm_get_max_height (int font_id)
{
  SIGNED_QUAD max = 0;
  int i;
  for (i=0; i<tfm[font_id].nheights; i++) {
    if ((tfm[font_id].height)[i] > max)
      max = (tfm[font_id].height)[i];
  }
  return (max/FWBASE);
}

double tfm_get_max_depth (int font_id)
{
  SIGNED_QUAD max = 0;
  int i;
  for (i=0; i<tfm[font_id].ndepths; i++) {
    if ((tfm[font_id].depth)[i] > max)
      max = (tfm[font_id].depth)[i];
  }
  return (max/FWBASE);
}
