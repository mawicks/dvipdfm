/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/ttf.c,v 1.5 1999/09/08 16:51:49 mwicks Exp $

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

/* This is tailored for PDF */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "system.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"
#include "numbers.h"
#include "ttf.h"
#include "tfm.h"
#include "pdfobj.h"
#include "pdfparse.h"
#include "pdflimits.h"

static const char *map_filename = "ttffonts.map";
static unsigned char verbose = 0;

void ttf_set_verbose(void)
{
  if (verbose < 255) {
    verbose += 1;
  }
}

struct font_record 
{
  char *enc_name;
  char *descriptive_name;
  char *ttf_name;
};

void ttf_set_mapfile (const char *name)
{
  map_filename = name;
  return;
}

struct font_record *ttf_get_font_record (const char *tex_name)
{
  struct font_record *result;
  static first = 1;
  static FILE *mapfile;
  char *full_map_filename, *start, *end = NULL, *record_name;
  result = NEW (1, struct font_record);
  result -> enc_name = NULL;
  result -> descriptive_name = NULL;
  result -> ttf_name = NULL;
  
  if (first) {
    first = 0;
    full_map_filename = kpse_find_file (map_filename, kpse_program_text_format,
					0);
    if (full_map_filename == NULL || 
	(mapfile = FOPEN (full_map_filename, FOPEN_R_MODE)) == NULL) {
      fprintf (stderr, "Warning:  No ttf font map file\n");
      mapfile = NULL;
    }
  }
  if (mapfile == NULL) 
    return result;
  rewind (mapfile);
  while ((start = mfgets (work_buffer, WORK_BUFFER_SIZE, mapfile)) !=
	 NULL) {
    end = work_buffer + strlen(work_buffer);
    skip_white (&start, end);
    if (start >= end)
      continue;
    if ((record_name = parse_ident (&start, end)) == NULL)
      continue;
    if (!strcmp (record_name, tex_name)) {
      RELEASE (record_name);
      break;
    }
    RELEASE (record_name);
  }
  if (start == NULL)
    return result;
  skip_white (&start, end);
  result -> enc_name = parse_ident (&start, end); /* May be null */  
  skip_white (&start, end);
  result -> descriptive_name = parse_ident (&start, end); /* May be null */
  skip_white (&start, end);
  result -> ttf_name = parse_ident (&start, end); /* May be null */
  return result;
}

/* TTF section */
static num_ttfs = 0;
static max_ttfs = 0;
struct a_ttf
{
  char *ttf_name;
  char *fontname;
  pdf_obj *direct, *indirect;
  char used_chars[256];
  int encoding_id;
} *ttfs = NULL;

static void clear_a_ttf (struct a_ttf *ttf)
{
  int i;
  for (i=0; i<256; i++) {
    (ttf->used_chars)[i] = 0;
  }
}

#include "standardenc.h"

static partial_enabled = 0;

void ttf_disable_partial (void)
{
  partial_enabled = 0;
}

static void dump_used( char *used_chars)
{
  int i;
  for (i=0; i<256; i++)
    fprintf (stderr, "(%d/%d)", i, used_chars[i]);
  return;
}

static pdf_obj *ttf_fontfile (int ttf_id) 
{
  if (ttd_id >= 0 && ttf_id < num_ttfs)
    return pdf_link_obj(ttfs[ttf_id].indirect);
  else
    return NULL;
}

static char *ttf_fontname (int pfb_id)
{
  if (ttf_id >= 0 && ttf_id < num_ttfs)
    return ttfs[ttf_id].fontname;
  else
    return NULL;
}

static int ttf_id (const char *ttf_name, int encoding_id, char *fontname)
{
  int i;
  for (i=0; i<num_pfbs; i++) {
    if (ttfs[i].ttf_name && !strcmp (ttfs[i].ttf_name, ttf_name))
      break;
  }
  if (i == num_ttfs) {
    char *full_ttf_name;
    full_ttf_name = kpse_find_file (ttf_name, kpse_type1_format,
				    1);
    if (full_ttf_name == NULL) {
      fprintf (stderr, "type1_fontfile:  Unable to find ttf font file (%s).", ttf_name);
      return -1;
    }
    if (num_ttfs >= max_ttfs) {
      max_ttfs += MAX_FONTS;
      ttfs = RENEW (ttfs, max_ttfs, struct a_ttf);
    }
    num_ttfs += 1;
    clear_a_ttf (ttfs+i);
    ttfs[i].ttf_name = NEW (strlen(ttf_name)+1, char);
    strcpy (ttfs[i].ttf_name, ttf_name);
    ttfs[i].direct = pdf_new_stream(STREAM_COMPRESS);
    ttfs[i].indirect = pdf_ref_obj (ttfs[i].direct);
    ttfs[i].encoding_id = encoding_id;
    if (partial_enabled) {
      ttfs[i].fontname = NEW (strlen(fontname)+8, char);
      strcpy (ttfs[i].fontname, fontname);
      mangle_fontname(ttfs[i].fontname);
    }
    else {
      ttfs[i].fontname = NEW (strlen(fontname)+1, char);
      strcpy (ttfs[i].fontname, fontname);
    }
  }
  return i;
}

static void release_glyphs (char **glyphs)
{
  int i;
  for (i=0; i<256; i++) {
    RELEASE (glyphs[i]);
  }
}

static void do_ttf (int ttf_id)
{
  char *full_ttf_name;
  FILE *ttf_file;
  pdf_obj *stream_dict;
  char *buffer;
  unsigned long length;
  int ch;
  full_ttf_name = kpse_find_file (ttfs[ttf_id].ttf_name, kpse_type1_format,
				  1);
  if (verbose) {
    fprintf (stderr, "(%s)", full_ttf_name);
  }
  if (full_ttf_name == NULL ||
      (ttf_file = FOPEN (full_ttf_name, FOPEN_RBIN_MODE)) == NULL) {
    fprintf (stderr, "ttf_do_ttf:  Unable to find or open binary font file (%s)",
	     ttfs[ttf_id].ttf_name);
    ERROR ("This existed when I checked it earlier!");
    return;
  }
  length = file_size (ttf_file);
  buffer = NEW (length, char *);
  fread (buffer, sizeof (char), length, ttf_file);
  pdf_add_stream (ttfs[ttf_id].direct, buffer, length);
  RELEASE (buffer);
  FCLOSE (ttf_file);
  stream_dict = pdf_stream_dict (pfbs[pfb_id].direct);
  pdf_add_dict (stream_dict, pdf_new_name("Length"),
		pdf_new_number (length));
  if (verbose > 1)
    fprintf (stderr, "Embedded size: %ld bytes\n", length);
  return;
}

#define FIXED_WIDTH 1
#define SERIF 2
#define STANDARD 32
#define ITALIC 64
#define SYMBOLIC 4   /* Fonts that don't have Adobe encodings (e.g.,
			cmr, should be set to be symbolic */
#define STEMV 80


static pdf_obj *ttf_font_descriptor (const char *ttf_name, int encoding_id,
				int ttf_id)
{
  pdf_obj *font_descriptor, *font_descriptor_ref, *tmp1;
  int flags;
  font_descriptor = pdf_new_dict ();
  pdf_add_dict (font_descriptor,
		pdf_new_name ("Type"),
		pdf_new_name ("FontDescriptor"));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("CapHeight"),
		pdf_new_number (ROUND(capheight,0.01)));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("Ascent"),
		pdf_new_number (ROUND(ascent,0.01)));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("Descent"),
		pdf_new_number (ROUND(descent,0.01)));
  flags = 0;
  if (italicangle != 0.0) flags += ITALIC;
  if (isfixed) flags += FIXED_WIDTH;
  flags += SYMBOLIC;
  pdf_add_dict (font_descriptor,
		pdf_new_name ("Flags"),
		pdf_new_number (flags));
  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, pdf_new_number (ROUND(bbllx,1)));
  pdf_add_array (tmp1, pdf_new_number (ROUND(bblly,1)));
  pdf_add_array (tmp1, pdf_new_number (ROUND(bburx,1)));
  pdf_add_array (tmp1, pdf_new_number (ROUND(bbury,1)));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("FontBBox"),
		tmp1);
  pdf_add_dict (font_descriptor,
		pdf_new_name ("FontName"),
		pdf_new_name (fontname));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("ItalicAngle"),
		pdf_new_number (ROUND(italicangle,1)));
  if (xheight != 0.0) {
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("XHeight"),
		  pdf_new_number (ROUND(xheight,1)));
  }
  pdf_add_dict (font_descriptor,
		pdf_new_name ("StemV"),  /* This is required */
		pdf_new_number (STEMV));
  /* You don't need a fontfile for the standard fonts */
  if (pfb_id >= 0)
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("FontFile"),
		  type1_fontfile (pfb_id));
  font_descriptor_ref = pdf_ref_obj (font_descriptor);
  pdf_release_obj (font_descriptor);
  return font_descriptor_ref;
}

static void fill_in_defaults (struct font_record *font_record, const
			      char *tex_name )
{
  if (font_record -> enc_name != NULL && 
      (!strcmp (font_record->enc_name, "default") ||
       !strcmp (font_record->enc_name, "none"))) {
    RELEASE(font_record->enc_name);
    font_record -> enc_name = NULL;
  }
  if (font_record -> descriptive_name != NULL && 
      (!strcmp (font_record->descriptive_name, "default") ||
       !strcmp (font_record->descriptive_name, "none"))) {
    RELEASE(font_record->descriptive_name);
    font_record -> descriptive_name = NULL;
  }
  /* We *must* fill in an descriptive_name either explicitly or by default */
  if (font_record -> descriptive_name == NULL) {
    font_record -> descriptive_name = NEW (strlen(tex_name)+1, char);
    strcpy (font_record->descriptive_name, tex_name);
  }
  /* If a ttf_name wasn't specified, default to tex_name */
  if (font_record -> ttf_name == NULL) {
    font_record -> ttf_name = NEW (strlen(tex_name)+1, char);
    strcpy (font_record->ttf_name, tex_name);
  }
  /* If a "none" pfb name was specified, set name to null */
  if (font_record -> ttf_name != NULL && 
      !strcmp (font_record->pfb_name, "none")) {
    ERROR ("\"none\" is invalid ttf file name\n");
    RELEASE(font_record->ttf_name);
    font_record -> ttf_name = NULL;
  }
  return;
}

struct a_ttf_font
{
  pdf_obj *indirect;
  int ttf_id;
} *ttf_fonts;
int num_ttf_fonts = 0, max_ttf_fonts = 0;


pdf_obj *ttf_font_resource (int ttf_id)
{
  if (ttf_id>=0 && type1_id<max_ttf_fonts)
    return pdf_link_obj(ttf_fonts[ttf_id].indirect);
  else {
    ERROR ("Invalid font id in type1_font_resource");
    return NULL;
  }
}

char *ttf_font_used (int ttf_id)
{
  int ttf_id;
  char *result;
  if (ttf_id>=0 && ttf_id<max_ttf_fonts &&
      (ttf_id = ttf_fonts[ttf_id].ttf_id) >= 0 &&
      (ttf_id <max_ttfs))
    result = ttfs[ttf_id].used_chars;
  else if (ttf_id >= 0 && ttf_id < max_ttf_fonts){
    result = NULL;
  } else {
    fprintf (stderr, "ttf_font_used: ttf_id=%d\n", ttf_id);
    ERROR ("Invalid font id in ttf_font_used");
  }
  return result;
}

int ttf_font (const char *tex_name, int ttf_font_id, const char *resource_name)
{
  int i, result = -1;
  int firstchar, lastchar;
  int encoding_id = -1;
  pdf_obj *font_resource, *tmp1, *font_encoding_ref;
  struct font_record *font_record;
  /* font_record should always result in non-null value with default
     values filled in from pdffonts.map if any */
  font_record = get_font_record (tex_name);
  /* Fill in default value for afm_name, enc, and pfb_name if not from
     map file */
  fill_in_defaults (font_record, tex_name);
  if (verbose>1){
    fprintf (stderr, "\nfontmap: %s -> %s/%s", tex_name,
	     font_record->descriptive_name,
	     font_record->ttf_name?font_record->ttf_name:"none");
    if (font_record->enc_name)
      fprintf (stderr, "(%s)", font_record->enc_name);
    fprintf (stderr, "\n");
  }
  if (open_afm_file (font_record ->afm_name)) { /* If we have an AFM
						   file, assume we
						   have a "physical"
						   font */
    scan_afm_file();
    close_afm_file ();
    {
      /* Make sure there is enough room in type1_fonts for this entry */
      if (num_type1_fonts >= max_type1_fonts) {
	max_type1_fonts += MAX_FONTS;
	type1_fonts = RENEW (type1_fonts, max_type1_fonts, struct a_type1_font);
      }
      font_resource = pdf_new_dict ();
      pdf_add_dict (font_resource,
		    pdf_new_name ("Type"),
		    pdf_new_name ("Font"));
      pdf_add_dict (font_resource,
		    pdf_new_name ("Subtype"),
		    pdf_new_name ("Type1"));
      pdf_add_dict (font_resource,
		    pdf_new_name ("Name"),
		    pdf_new_name (resource_name));
      if (font_record -> enc_name != NULL) {
	encoding_id = get_encoding (font_record -> enc_name);
	font_encoding_ref = pdf_link_obj(encodings[encoding_id].encoding_ref);
	pdf_add_dict (font_resource,
		      pdf_new_name ("Encoding"),
		      font_encoding_ref);
      }
      /* Assume there will be no pfb for this font */
      type1_fonts[num_type1_fonts].pfb_id = -1;
      if (font_record -> pfb_name != NULL) {
	if (!is_a_base_font (fontname)) {
	  type1_fonts[num_type1_fonts].pfb_id =
	    type1_pfb_id (font_record -> pfb_name, encoding_id, fontname);
	  pdf_add_dict (font_resource, 
			pdf_new_name ("FontDescriptor"),
			type1_font_descriptor(font_record -> pfb_name,
					      encoding_id,
					      type1_fonts[num_type1_fonts].pfb_id));
	}
      }
      /* If we are embedding this font, it may have been used by another virtual
	 font and we need to use the same mangled name.  Mangled
	 named are known only to the pfb module */
      if (type1_fonts[num_type1_fonts].pfb_id >= 0) {
	pdf_add_dict (font_resource, 
		      pdf_new_name ("BaseFont"),
		      pdf_new_name
		      (type1_fontname(type1_fonts[num_type1_fonts].pfb_id)));
	/* Otherwise we use the base name */
      } else {
	pdf_add_dict (font_resource,
		      pdf_new_name ("BaseFont"),
		      pdf_new_name (fontname));  /* fontname is global and set
						    by scan_afm_file() */
      }
      firstchar = tfm_get_firstchar(tfm_font_id);
      pdf_add_dict (font_resource,
		    pdf_new_name ("FirstChar"),
		    pdf_new_number (firstchar));
      lastchar = tfm_get_lastchar(tfm_font_id);
      pdf_add_dict (font_resource,
		    pdf_new_name ("LastChar"),
		    pdf_new_number (lastchar));
      tmp1 = pdf_new_array ();
      for (i=firstchar; i<=lastchar; i++) {
	pdf_add_array (tmp1,
		       pdf_new_number(ROUND(tfm_get_width (tfm_font_id, i)*1000.0,0.01)));
      }
      pdf_add_dict (font_resource,
		    pdf_new_name ("Widths"),
		    tmp1);
      type1_fonts[num_type1_fonts].indirect = pdf_ref_obj(font_resource);
      pdf_release_obj (font_resource);
      result = num_type1_fonts;
      num_type1_fonts += 1;
    }
  } else { /* No AFM file.  This isn't fatal.  We still might have a vf! */
    result = -1;
  }
  if (font_record -> enc_name)
    RELEASE (font_record -> enc_name);
  if (font_record -> afm_name)
    RELEASE (font_record -> afm_name);
  if (font_record -> pfb_name)
    RELEASE (font_record -> pfb_name);
  RELEASE (font_record);
  return result;
}

void type1_close_all (void)
{
  int i, j;
  /* Three arrays are created by this module and need to be released */

  /* First, each TeX font name that ends up as a postscript font gets
     added to type1_fonts (yes, even times, etc.) */
  for (i=0; i<num_type1_fonts; i++) {
    pdf_release_obj (type1_fonts[i].indirect);
  }
  RELEASE (type1_fonts);
  /* Second every distinct pfb name ends up in pfbs.  It is possible
     that two distinct tex names map to the same pfb name.  That's why
     there is a separate array for pfbs */

  /* Read any necessary font files and flush them */
  for (i=0; i<num_pfbs; i++) {
    do_pfb (i);
    pdf_release_obj (pfbs[i].direct);
    RELEASE (pfbs[i].pfb_name);
    pdf_release_obj (pfbs[i].indirect);
    RELEASE (pfbs[i].fontname);
  }
  RELEASE (pfbs);
  /* Now do encodings.  Clearly many pfbs will map to the same
     encoding.  That's why there is a separate array for encodings */
  for (i=0; i<num_encodings; i++) {
    RELEASE (encodings[i].enc_name);
    pdf_release_obj (encodings[i].encoding_ref);
    /* Release glyph names for this encoding */
    for (j=0; j<256; j++) {
      RELEASE ((encodings[i].glyphs)[j]);
    }
  }
  if (encodings)
     RELEASE (encodings);
}
