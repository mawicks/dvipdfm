/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/type1.c,v 1.98 1999/10/08 15:39:34 mwicks Exp $

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
#include <math.h>
#include <time.h>
#include <ctype.h>
#include "system.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"
#include "pdfobj.h"
#include "numbers.h"
#include "type1.h"
#include "tfm.h"
#include "pdfparse.h"
#include "pdflimits.h"
#include "t1crypt.h"
#include "twiddle.h"

#define DEFAULT_MAP_FILE "t1fonts.map"
static unsigned char verbose = 0, really_quiet = 0;

void type1_set_verbose(void)
{
  if (verbose < 255) {
    verbose += 1;
  }
}

void type1_set_quiet (void)
{
  really_quiet = 1;
}

struct font_record 
{
  char *tex_name;
  char *font_name;
  char *enc_name;
  double slant, extend;
  int remap;
} *font_map = NULL;

unsigned int num_font_map = 0, max_font_map = 0;

static void need_more_font_maps (int n)
{
  if (num_font_map+n > max_font_map) {
    max_font_map += MAX_FONTS;
    font_map = RENEW (font_map, max_font_map, struct font_record);
  }
  return;
}

static void init_font_record (struct font_record *r) 
{
  r->tex_name = NULL;
  r->enc_name = NULL;
  r->font_name = NULL;
  r->slant = 0.0;
  r->extend = 1.0;
  r->remap = 0.0;
  return;
}
static void fill_in_defaults (struct font_record *font_record)
{
  if (font_record -> enc_name != NULL && 
      (!strcmp (font_record->enc_name, "default") ||
       !strcmp (font_record->enc_name, "none"))) {
    RELEASE(font_record->enc_name);
    font_record -> enc_name = NULL;
  }
  if (font_record -> font_name != NULL && 
      (!strcmp (font_record->font_name, "default") ||
       !strcmp (font_record->font_name, "none"))) {
    RELEASE(font_record->font_name);
    font_record -> font_name = NULL;
  }
  /* We *must* fill in a font_name either explicitly or by default */
  if (font_record -> font_name == NULL) {
    font_record -> font_name = NEW (strlen(font_record->tex_name)+1, char);
    strcpy (font_record->font_name, font_record->tex_name);
  }
  return;
}

struct encoding {
  char *enc_name;
  /* The following array isn't very efficient. It is constructed
     by peeling the names from the encoding object.  It makes
     it easier to construct an array in this format when the
     encoding must be obtained directly from the PFB file */
  char *glyphs[256];
  pdf_obj *encoding_ref;
} *encodings;
int num_encodings = 0, max_encodings=0;

#include "winansi.h"

pdf_obj *find_encoding_differences (pdf_obj *encoding)
{
  static char filling = 0;
  int i;
  pdf_obj *result = pdf_new_array ();
  pdf_obj *tmp;
  for (i=0; i<256; i++) {
    tmp = pdf_get_array (encoding, i);
    if (tmp == NULL || tmp -> type != PDF_NAME) {
      ERROR ("Encoding file may be incorrect\n");
    }
    if (!strcmp (winansi_encoding[i],
		 pdf_name_value(tmp)))
      filling = 0;
    else{
      if (!filling)
	pdf_add_array (result, pdf_new_number (i));
      filling = 1;
      pdf_add_array (result, pdf_link_obj(tmp));
    }
  }
  return result;
}

static void save_glyphs (char **glyph, pdf_obj *encoding)
{
  int i;
  char *glyph_name;
  for (i=0; i<256; i++) {
    glyph_name = pdf_string_value (pdf_get_array(encoding, i));
    glyph[i] = NEW (strlen(glyph_name)+1, char);
    strcpy (glyph[i], glyph_name);
  }
}

int get_encoding (const char *enc_name)
{
  FILE *encfile = NULL;
  char *full_enc_filename, *tmp;
  pdf_obj *result, *result_ref;
  long filesize;
  int i;
  /* See if we already have this saved */
  for (i=0; i<num_encodings; i++)
    if (!strcmp (enc_name, encodings[i].enc_name))
      return i;
  /* Guess not.. */
  /* Try base name before adding .enc.  Someday maybe kpse will do
     this */
  strcpy (tmp = NEW (strlen(enc_name)+5, char), enc_name);
  strcat (tmp, ".enc");
  if ((full_enc_filename = kpse_find_file (enc_name,
					   kpse_tex_ps_header_format,
					   1)) == NULL &&
      (full_enc_filename = kpse_find_file (enc_name,
					   kpse_program_text_format,
					   1)) == NULL &&
      (full_enc_filename = kpse_find_file (tmp,
					   kpse_tex_ps_header_format,
					   1)) == NULL &&
      (full_enc_filename = kpse_find_file (tmp,
					   kpse_program_text_format,
					   1)) == NULL) {
    sprintf (work_buffer, "Can't find encoding file: %s", enc_name) ;
    ERROR (work_buffer);
  }
  RELEASE (tmp);
  if ((encfile = FOPEN (full_enc_filename, FOPEN_R_MODE)) == NULL ||
      (filesize = file_size (encfile)) == 0) {
    sprintf (work_buffer, "Error opening encoding file: %s", enc_name) ;
    ERROR (work_buffer);
  }
  if (verbose) {
    fprintf (stderr, "(%s", full_enc_filename);
  }
  {  /* Got one and opened it */
    char *buffer, *start, *end, *junk_ident;
    pdf_obj *junk_obj, *encoding, *differences;
    buffer = NEW (filesize, char); 
    fread (buffer, sizeof (char), filesize, encfile);
    FCLOSE (encfile);
    start = buffer;
    end = buffer + filesize;
    start[filesize-1] = 0;
    skip_white (&start, end);
    while (start < end && *start != '[') {
      if ((junk_ident = parse_ident (&start, end)) != NULL)
	RELEASE (junk_ident);
      else if ((junk_obj = parse_pdf_object (&start, end)) != NULL)
	pdf_release_obj (junk_obj);
      skip_white(&start, end);
    }
    if (start >= end ||
	(encoding = parse_pdf_array (&start, end)) == NULL) {
      fprintf (stderr, "%s: ", enc_name);
      ERROR ("Can't find an encoding in this file!\n");
    }
    RELEASE (buffer);
    /* Done reading file */
    if (verbose) {
      fprintf (stderr, ")");
    }
    differences = find_encoding_differences (encoding);
    /* Put the glyph names into a conventional array */
    if (num_encodings >= max_encodings) {
       max_encodings += MAX_ENCODINGS;
       encodings = RENEW (encodings, max_encodings, struct encoding);
    }
    save_glyphs (encodings[num_encodings].glyphs, encoding);
    pdf_release_obj (encoding);
    result = pdf_new_dict();
    pdf_add_dict (result, pdf_new_name ("Type"),
		  pdf_new_name ("Encoding"));
    pdf_add_dict (result, pdf_new_name ("BaseEncoding"),
		  pdf_new_name ("WinAnsiEncoding"));
    pdf_add_dict (result, pdf_new_name ("Differences"),
		  differences);
  }
  {
    result_ref = pdf_ref_obj (result);
    pdf_release_obj (result);
    encodings[num_encodings].encoding_ref = result_ref;
    encodings[num_encodings].enc_name = NEW (strlen(enc_name)+1, char);
    strcpy (encodings[num_encodings].enc_name, enc_name);
    return num_encodings++;
  }
}

extern void type1_read_mapfile (char *filename)
{
  FILE *mapfile;
  char *full_map_filename, *start = NULL, *end, *tex_name;
  if (!really_quiet)
    fprintf (stderr, "<%s", filename);
  full_map_filename = kpse_find_file (filename, kpse_program_text_format,
				      0);
  if (full_map_filename == NULL || 
      (mapfile = FOPEN (full_map_filename, FOPEN_R_MODE)) == NULL) {
    fprintf (stderr, "Warning:  Couldn't open font map file %s\n", filename);
    mapfile = NULL;
  }
  if (mapfile) {
    while ((start = mfgets (work_buffer, WORK_BUFFER_SIZE, mapfile)) !=
	   NULL) {
      end = work_buffer + strlen(work_buffer);
      skip_white (&start, end);
      if (start >= end)
	continue;
      if (*start == '%')
	continue;
      if ((tex_name = parse_ident (&start, end)) == NULL)
	continue;
      /* Parse record line in map file.  First two fields (after TeX font
	 name) are position specific.  Arguments start at the first token
	 beginning with a  '-' */
      need_more_font_maps (1);
      init_font_record(font_map+num_font_map);
      font_map[num_font_map].tex_name = tex_name;
      skip_white (&start, end);
      if (*start != '-') {
	font_map[num_font_map].enc_name = parse_ident (&start, end); /* May be null */  
	skip_white (&start, end);
      }
      if (*start != '-') {
	font_map[num_font_map].font_name = parse_ident (&start, end); /* May be null */
	skip_white (&start, end);
      }
      /* Parse any remaining arguments */ 
      while (start+1 < end && *start == '-') {
	char *number;
	switch (*(start+1)) {
	case 's': /* Slant option */
	  start += 2;
	  skip_white (&start, end);
	  if (start < end && 
	      (number = parse_number(&start, end))) {
	    font_map[num_font_map].slant = atof (number);
	    RELEASE (number);
	  } else {
	    fprintf (stderr, "\n\nMissing slant value in map file for %s\n\n",
		     tex_name);
	  }
	  break;
	case 'e': /* Extend option */
	  start += 2;
	  skip_white (&start, end);
	  if (start < end && 
	      (number = parse_number(&start, end))) {
	    font_map[num_font_map].extend = atof (number);
	    RELEASE (number);
	  } else {
	    fprintf (stderr, "\n\nMissing extend value in map file for %s\n\n",
		     tex_name);
	  }
	  break;
	case 'r': /* Remap option */
	  start += 2;
	  skip_white (&start, end);
	  font_map[num_font_map].remap = 1;
	  break;
	default: 
	  fprintf (stderr, "\n\nWarning: Unrecognized option in map file %s: -->%s<--\n\n",
		   tex_name, start);
	  start = end;
	}
	skip_white (&start, end);
      }
      fill_in_defaults (font_map+num_font_map);
      num_font_map += 1;
    }
    FCLOSE (mapfile);
    if (!really_quiet)
      fprintf (stderr, ">");
  }
  return;
}

struct font_record *get_font_record (const char *tex_name)
{
  struct font_record *result = NULL;
  unsigned int i;
  if (!font_map) {
    type1_read_mapfile (DEFAULT_MAP_FILE);
  }
  if (!font_map)
    return result;
  for (i=0; i<num_font_map; i++) {
    if (!strcmp (font_map[i].tex_name, tex_name)) {
      result = font_map+i;
      break;
    }
  }
  return result;
}

static unsigned long get_low_endian_quad (FILE *file)
{
  unsigned long result;
  static unsigned bytes[4];
  int ch, i;
  for (i=0; i<4; i++) {
    if ((ch = fgetc (file)) < 0) {
      ERROR ("get_low_endian_quad:  Error reading file");
    }
    bytes[i] = ch;
  }
  result = bytes[3];
  for (i=2; i>=0; i--) {
    result = result*256u + bytes[i];
  }
  return result;
}

/* PFB section */
static unsigned num_pfbs = 0;
static unsigned max_pfbs = 0;
struct a_pfb
{
  char *pfb_name;
  char *fontname;
  pdf_obj *direct, *indirect;
  char *used_chars;
  int encoding_id, remap;
} *pfbs = NULL;

static void init_a_pfb (struct a_pfb *pfb)
{
  int i;
  pfb -> used_chars = NEW (256, char);
  for (i=0; i<256; i++) {
    (pfb->used_chars)[i] = 0;
  }
  pfb->remap = 0;
  pfb->pfb_name = NULL;
  pfb->fontname = NULL;
  pfb->direct = NULL;
  pfb->indirect = NULL;
  pfb->encoding_id = -1;
}

#include "standardenc.h"

static void do_a_standard_enc(char **glyphs, char **encoding) 
{
  int i;
  for (i=0; i<256; i++) {
    RELEASE (glyphs[i]);
    glyphs[i] = NEW (strlen(encoding[i])+1, char);
    strcpy (glyphs[i], encoding[i]);
  }
}

static char partial_enabled = 1;

void type1_disable_partial (void)
{
  partial_enabled = 0;
}

static unsigned long parse_header (unsigned char *filtered, unsigned char *buffer,
			  unsigned long length, int pfb_id,
			  char **glyphs)
{
  /* This routine must work correctly if glyphs == NULL.  In this
     case, the encoding has been specified in the pdffonts.map file
     and we should eliminate any built-in encoding (other than things
     like StandardEncoding) in the header to
     save space */

  /* On second thought, that's the way it _should_ work, but the
     reader doesn't seem to agree.  The reader is happy if you don't
     specify an encoding as long as you actually change the locations
     in the overriding encoding. The reader is unhappy if you don't
     specify an encoding and don't change the location of the
     characters in the overriding encoding.  Ghostscript doesn't
     have a problem with it. */

  unsigned char *filtered_pointer;
  int state = 0;
  char *start, *end, *lead, *saved_lead = NULL;
  int last_number = 0;
  char *glyph = NULL;
#ifdef MEM_DEBUG
  MEM_START
#endif
  if (glyphs) {
    int i;
    for (i=0; i<256; i++) {
      glyphs[i] = NEW (strlen (".notdef")+1, char); 
      strcpy (glyphs[i], ".notdef");
    }
  }
  /* State definitions 
     state 0: Initial state
     state 1: Saw /FontName
     state 2: Saw /Encoding
     state 3: Saw "dup" in state 2
     state 4: Saw a number in state 3
     state 5: Saw a /glyphname in state 4 */
  start = (char *) buffer;
  end = start+length;
  filtered_pointer = filtered;
  lead = start;
  skip_white (&start, end);
  if (lead != start) {
    memcpy (filtered_pointer, lead, start-lead);
    filtered_pointer += start-lead;
  }
  while (start < end) {
    char *save, *ident;
    pdf_obj *pdfobj;
    switch (state) {
      /* First three states are very similar.  In most cases we just
	 ignore other postscript junk and don't change state */
    case 0:
    case 1:
    case 2:
      lead = start;
      switch (*start) {
	/* Ignore arrays and procedures */
      case '[':
      case ']':  
      case '{':
      case '}':
	start += 1;
	if (state >= 2) 
	  state = 2;
	break;
      case '(':
	pdfobj = parse_pdf_string (&start, end);
	if (pdfobj == NULL) {
	  ERROR ("parse_header:  Error parsing a string in pfb header");
	}
	pdf_release_obj (pdfobj);
	if (state == 1) {
	  filtered_pointer += sprintf ((char *)filtered_pointer, "/%s ",
				       pfbs[pfb_id].fontname);
	  lead = NULL; /* Means don't copy current input to output */
	  state = 0;
	}
	if (state >= 2)
	  state = 2;
	break;
      case '/':
	start += 1;
	ident = parse_ident (&start, end);
	if (state == 0 && !strcmp (ident, "FontName")) {
	  state = 1;
	} else if (state == 0 && !strcmp (ident, "Encoding")) {
	  state = 2;
	} else if (state == 1) {
	  filtered_pointer += sprintf ((char *)filtered_pointer, "/%s ",
				       pfbs[pfb_id].fontname);
	  lead = NULL; /* Means don't copy current input to output */
	  state = 0;
	}
	RELEASE (ident);
	break;
      default:
	save = start;
	ident = parse_ident (&start, end);
	if (state == 2 &&
	    !strcmp (ident, "dup")) {
	  saved_lead = save; /* Save this because we may need it
				 later */
	  lead = NULL;
	  state = 3;
	} else if (state == 2 &&
		   !strcmp (ident, "StandardEncoding") && glyphs) {
	  do_a_standard_enc(glyphs, standardencoding);
	} else if (state == 2 &&
		   !strcmp (ident, "ISOLatin1Encoding") && glyphs) {
	  do_a_standard_enc(glyphs, isoencoding);
	}
	RELEASE (ident);
	break;
      }
      break;
    case 3:
      ident = parse_ident (&start, end);
      if (is_a_number (ident)) {
	last_number = (int) atof (ident);
	state = 4;
      } else {
	lead = saved_lead;
	state = 2;
      }
      RELEASE (ident);
      break;
    case 4:
      if (*start == '/') {
	start += 1;
	glyph = parse_ident (&start, end);
	state = 5;
      } else {
	lead = saved_lead;
	state = 2;
      }
      break;
    case 5:
      ident = parse_ident (&start, end);
      /* Here we either decide to keep or remove the encoding entry */
      if (ident != NULL && !strcmp (ident, "put") && 
	  (int) last_number < 256 && (int) last_number >= 0) {
	if ((pfbs[pfb_id].used_chars)[last_number]) {
	  filtered_pointer += 
	    sprintf((char *) filtered_pointer, "dup %d /%s put\n",
		    pfbs[pfb_id].remap?twiddle(last_number):last_number,
		    glyph);
	}
	if (glyphs) {
	  if (glyphs[last_number] != NULL) 
	    RELEASE (glyphs[last_number]);
	  glyphs[last_number] = glyph;
	  glyph = NULL; /* Don't free glyph later */
	}
      } else {
	lead = saved_lead;
      }
      if (glyph)
	RELEASE (glyph);
      if (ident != NULL)
	RELEASE (ident);
      state = 2;
      break;
    }
    skip_white (&start, end);
    if (lead) {
      memcpy (filtered_pointer, lead, start-lead);
      filtered_pointer += start-lead;
      lead = start;
    }
  }
#ifdef MEM_DEBUG
  MEM_END
#endif /* MEM_DEBUG */
  return filtered_pointer-filtered;
}


static void dump_glyphs( char **glyphs)
{
  int i;
  for (i=0; i<256; i++)
    fprintf (stderr, "(%d/%s)", i, glyphs[i]);
  return;
}
static void dump_used( char *used_chars)
{
  int i;
  for (i=0; i<256; i++)
    fprintf (stderr, "(%d/%d)", i, used_chars[i]);
  return;
}

#define ASCII 1
#define BINARY 2

static unsigned char *get_pfb_segment (unsigned long *length,
				       FILE *file, int expected_type)
{
  unsigned char *buffer = NULL;
  unsigned long nread;
  unsigned long new_length;
  int stream_type, ch;

  *length = 0;
  /* Unfortunately, there can be several segments that need to be
     concatenated, so we loop through all of them */
  while (1) {
    if ((ch = fgetc (file)) < 0 || ch != 128){
      sprintf (work_buffer, "get_pfb_segment:  Not a pfb file.\n");
      sprintf (work_buffer, "get_pfb_segment:  pfb header has %d, expecting 128\n", ch);
      ERROR (work_buffer);
    }
    if ((stream_type = fgetc (file)) < 0 || stream_type != expected_type) {
      seek_relative (file, -2); /* Backup up two (yuck!) */
      break;
    }
    new_length = get_low_endian_quad (file);
    if (verbose > 3) {
      fprintf (stderr, "Length of next PFB segment: %ld\n",
	       new_length);
    }
    buffer = RENEW (buffer, (*length)+new_length, unsigned char);
    if ((nread = fread(buffer+(*length), sizeof(unsigned char), new_length, file)) !=
	new_length) {
      fprintf (stderr, "Found only %ld/%ld bytes\n", nread, new_length);
      ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
    }
    *length += new_length;
  }
  if (*length == 0) {
    ERROR ("type1_get_pfb_segment: Segment length is zero");
  }
  if (expected_type == ASCII) {
    int i;
    for (i=0; i<(*length); i++) {
      if (buffer[i] == '\r')
	buffer[i] = '\n';  /* Show my Unix prejudice */
    }
  }
  return buffer;
}

static unsigned int glyph_length (char **glyphs) 
{
  int i;
  unsigned result = 0;
  for (i=0; i<256; i++) {
    result += strlen (glyphs[i]);
  }
  return result;
}

static char *pfb_find_name (FILE *pfb_file) 
{
  unsigned char *buffer;
  unsigned long length = 0;
  char *start, *end, *fontname;
  int state = 0;
#ifdef MEM_DEBUG
  MEM_START
#endif
  buffer = get_pfb_segment (&length, pfb_file, ASCII);
  /* State definitions 
     state 0: Initial state
     state 1: Saw /FontName */
  start = (char *) buffer;
  end = start+length;
  skip_white (&start, end);
  fontname = NULL;
  while (start < end && fontname == NULL) {
    char *ident;
    pdf_obj *pdfobj;
    switch (*start) {
      /* Ignore arrays and procedures */
    case '[':
    case ']':  
    case '{':
    case '}':
      start += 1;
      if (state == 1) {
	ERROR ("Garbage following /FontName");
      }
      break;
    case '(':
      pdfobj = parse_pdf_string (&start, end);
      if (pdfobj == NULL) {
	ERROR ("parse_header:  Error parsing a string in pfb header");
      }
      if (state == 1) { /* This string must be the font name */
	char *tmp = pdf_string_value (pdfobj);
	fontname = NEW (strlen(tmp)+1, char);
	memcpy (fontname, tmp, strlen(tmp)+1);
      }
      pdf_release_obj (pdfobj);
      break;
    case '/':
      start += 1;
      ident = parse_ident (&start, end);
      if (state == 0 && !strcmp (ident, "FontName")) {
	state = 1;
      } else if (state == 1) {
	fontname = NEW (strlen(ident)+1, char);
	memcpy (fontname, ident, strlen(ident)+1);
      }
      RELEASE (ident);
      break;
    default:
      ident = parse_ident (&start, end);
      RELEASE (ident);
      break;
    }
    skip_white (&start, end);
  }
  RELEASE (buffer);
#ifdef MEM_DEBUG
  MEM_END
#endif /* MEM_DEBUG */
  return fontname;
}


static unsigned long do_pfb_header (FILE *file, int pfb_id,
				    char **glyphs)
{
  unsigned char *buffer, *filtered;
  unsigned long length = 0;
#ifdef MEM_DEBUG
MEM_START
#endif
  buffer = get_pfb_segment (&length, file, ASCII);
  if (partial_enabled) {
    filtered = NEW (length+strlen(pfbs[pfb_id].fontname)+1+1024, unsigned
		    char);
    length = parse_header (filtered, buffer, length, pfb_id, glyphs);
    pdf_add_stream (pfbs[pfb_id].direct, (char *) filtered, length);
    RELEASE (filtered);
  } else {
    pdf_add_stream (pfbs[pfb_id].direct, (char *) buffer, length);
  }
  RELEASE (buffer);
#ifdef MEM_DEBUG
MEM_END
#endif
  return length;
}

int glyph_cmp (const void *v1, const void *v2)
{
  char *s1, *s2;
  s1 = *((char **) v1);
  s2 = *((char **) v2);
  return (strcmp (s1, s2));
}

int glyph_match (const void *key, const void *v)
{
  char *s;
  s = *((char **) v);
  return (strcmp (key, s));
}

static unsigned long do_partial_body (unsigned char *filtered, unsigned char
				      *unfiltered, unsigned long length, 
				      char *chars_used, char **glyphs)
{
  char *start, *end, *tail, *ident;
  unsigned char *filtered_pointer;
  double last_number = 0.0;
  start = (char *) unfiltered, end = (char *) unfiltered+length;
  /* Skip first four bytes */
  tail = start; filtered_pointer = filtered;
  start += 4;
  /* Skip everything up to the charstrings section */
  while (start < end) {
    pdf_obj *pdfobj;
    skip_white (&start, end);
    switch (*start) {
    case '[':
    case ']':
    case '{':
    case '}':
      start += 1;
      continue;
    case '(':
      pdfobj = parse_pdf_string (&start, end);
      if (pdfobj == NULL) {
	ERROR ("Error processing a string in a PFB file.");
      }
      pdf_release_obj (pdfobj);
      continue;
    case '/':
      start += 1;
      ident = parse_ident (&start, end);
      if (ident == NULL)
	ERROR ("Error processing a name in a PFB file.");
      if (!strcmp ((char *) ident, "CharStrings")) {
	RELEASE (ident);
	break;
      }
      else {
	RELEASE (ident);
	continue;
      }
    default:
      ident = parse_ident (&start, end);
      if (ident == NULL)
	ERROR ("Error processing a symbol in the PFB file.");
      if (is_a_number(ident))
	last_number = atof (ident);
      else {
	if (!strcmp (ident, "RD") ||
	    !strcmp (ident, "-|")) {
	  start += ((unsigned long) last_number) + 1;
	}
      }
      RELEASE (ident);
      continue;
    }
    break;
  }
  if (start >= end)
    ERROR ("Unexpected end of binary portion of PFB file");
  /* Copy what we have so far over to the new buffer */
  memcpy (filtered_pointer, tail, start-tail);
  /* Advance pointer into new buffer */
  filtered_pointer += (start-tail);
  /* At this point, start is positioned just before the beginning of the glyphs, just after
     the word /CharStrings.  The earlier portion of the input buffer has
     been copied to the output */
  {
    int nused = 0;
    int nleft;
    static char *used_glyphs[256];
    struct glyph *this_glyph;
    int i;
    /* Build an array containing only those glyphs we need to embed */
    for (i=0; i<256; i++) {
      /* Don't add any glyph twice */
      if (chars_used[i] &&
	  (nused == 0 || /* Calling bsearch is unecessary if nused==0 */
	  !bsearch (glyphs[i], used_glyphs, nused, sizeof (char *), glyph_match))) {
	used_glyphs[nused] = glyphs[i];
	nused += 1;
	qsort (used_glyphs, nused, sizeof (char *), glyph_cmp);
      }
    }
    /* Add .notdef if it's not already there. We always embed .notdef */
    if (!bsearch (".notdef", used_glyphs, nused, sizeof (char *),
		  glyph_match)) {
      /* We never free the entries of used_glyphs so we can use a
	 static string */
      used_glyphs[nused] = ".notdef";
      nused += 1;
      qsort (used_glyphs, nused, sizeof (char *), glyph_cmp); 
    }
    nleft = nused;
    filtered_pointer += sprintf ((char *) filtered_pointer, " %d",
				 nused);
    skip_white(&start, end);
    /* The following ident *should* be the number of glyphs in this
       file */
    ident = parse_ident (&start, end);
    if (verbose>1) {
      fprintf (stderr, "\nEmbedding %d of %s glyphs", nused,ident);
    }
    if (ident == NULL || !is_a_number (ident) || nused > atof (ident)) 
      ERROR ("More glyphs needed than present in file");
    RELEASE (ident);
    tail = start;
    while (start < end && *start != '/') start++;
    memcpy (filtered_pointer, tail, start-tail);
    filtered_pointer += (start-tail);
    /* Now we are exactly at the beginning of the glyphs */
    while (start < end && *start == '/') {
      char *glyph;
      tail = start;
      start += 1;
      glyph = parse_ident (&start, end);
      this_glyph = bsearch (glyph, used_glyphs, nused, sizeof (char *), glyph_match);
      /* Get the number that should follow the glyph name */
      skip_white(&start, end);
      ident = parse_ident (&start, end);
      if (!is_a_number (ident))
	ERROR ("Expecting a number after glyph name");
      last_number = atof (ident);
      RELEASE (ident);
      /* The next identifier should be a "RD" or a "-|".  We don't
	 really care what it is */
      skip_white(&start, end);
      ident = parse_ident (&start, end);
      RELEASE (ident);
      /* Skip a blank */
      start += 1;
      /* Skip the binary stream */
      start += (unsigned long) last_number;
      /* Skip the "ND" or "|-" terminator */
      ident = parse_ident (&start, end);
      RELEASE (ident);
      skip_white (&start, end);
      if (this_glyph) {
	memcpy (filtered_pointer, tail, start-tail);
	filtered_pointer += start-tail;
	nleft--;
      }
      RELEASE (glyph);
    }
    if (nleft != 0)
      ERROR ("Didn't find all the required glyphs in the font.\nPossibly the encoding is incorrect.");
    /* Include the rest of the file verbatim */
    if (start < end){
      memcpy (filtered_pointer, start, end-start);
      filtered_pointer += end-start;
    }
  }
  if (verbose>1) {
    fprintf (stderr, " (eliminated %ld bytes)\n", length-(filtered_pointer-filtered));
  }
  return (filtered_pointer-filtered);
}



static unsigned long do_pfb_body (FILE *file, int pfb_id,
				  char **glyphs)
{
  int i;
  unsigned char *buffer=NULL, *filtered=NULL;
  unsigned long length=0;
#ifdef MEM_DEBUG
  MEM_START
#endif
  buffer = get_pfb_segment (&length, file, BINARY);
  if (partial_enabled && glyphs != NULL) {
    /* For partial font embedding we need to decrypt the binary
       portion of the pfb */
    t1_crypt_init(EEKEY);
    for (i=0; i<length; i++) {
      buffer[i] = t1_decrypt(buffer[i]);
    }
    /* Now we do the partial embedding */
    filtered = NEW (length, unsigned char);
    length = do_partial_body (filtered, buffer, length, 
			      pfbs[pfb_id].used_chars,
			      glyphs);
    /* And reencrypt the whole thing */
    t1_crypt_init (EEKEY);
    for (i=0; i<length; i++) {
      buffer[i] = t1_encrypt(filtered[i]);
    }
    RELEASE (filtered);
  }
  pdf_add_stream (pfbs[pfb_id].direct, (char *) buffer, length);
  RELEASE (buffer);
#ifdef MEM_DEBUG
  MEM_START
#endif
  return length;
}

static unsigned long do_pfb_trailer (FILE *file, pdf_obj *stream)
{
  unsigned char *buffer;
  unsigned long length;
  buffer = get_pfb_segment (&length, file, ASCII);
  pdf_add_stream (stream, (char *) buffer, length);
  RELEASE (buffer);
  return length;
}


static pdf_obj *type1_fontfile (int pfb_id) 
{
  if (pfb_id >= 0 && pfb_id < num_pfbs)
    return pdf_link_obj(pfbs[pfb_id].indirect);
  else
    return NULL;
}

static char *type1_fontname (int pfb_id)
{
  if (pfb_id >= 0 && pfb_id < num_pfbs)
    return pfbs[pfb_id].fontname;
  else
    return NULL;
}

/* Mangle_fontname mangles the name in place.  fontname
   must be big enough to add seven characters */

static void mangle_fontname(char *fontname)
{
  int i;
  char ch;
  static char first = 1;
  memmove (fontname+7, fontname, strlen(fontname)+1);
  /* The following procedure isn't very random, but it
     doesn't need to be for this application. */
  if (first) {
    srand (time(NULL));
    first = 0;
  }
  for (i=0; i<6; i++) {
    ch = rand() % 26;
    fontname[i] = ch+'A';
  }
  fontname[6] = '+';
}

static int type1_pfb_id (const char *pfb_name, int encoding_id, int remap)
{
  int i;
  for (i=0; i<num_pfbs; i++) {
    if (pfbs[i].pfb_name && !strcmp (pfbs[i].pfb_name, pfb_name))
      break;
  }
  if (i == num_pfbs) { /* This font not previously called for */
    FILE *pfb_file;
    char *full_pfb_name, *short_fontname;
    if (!(full_pfb_name = kpse_find_file (pfb_name, kpse_type1_format,
				    1)) || 
	!(pfb_file = FOPEN (full_pfb_name, FOPEN_RBIN_MODE))) {
      return -1;
    }
    short_fontname = pfb_find_name (pfb_file);
    FCLOSE (pfb_file);
    if (num_pfbs >= max_pfbs) {
      max_pfbs += MAX_FONTS;
      pfbs = RENEW (pfbs, max_pfbs, struct a_pfb);
    }
    num_pfbs += 1;
    init_a_pfb (pfbs+i);
    pfbs[i].pfb_name = NEW (strlen(pfb_name)+1, char);
    strcpy (pfbs[i].pfb_name, pfb_name);
    pfbs[i].direct = pdf_new_stream(STREAM_COMPRESS);
    pfbs[i].indirect = pdf_ref_obj (pfbs[i].direct);
    pfbs[i].encoding_id = encoding_id;
    if (partial_enabled) {
      pfbs[i].fontname = NEW (strlen(short_fontname)+8, char);
      strcpy (pfbs[i].fontname, short_fontname);
      mangle_fontname(pfbs[i].fontname);
      pfbs[i].remap = remap;
    }
    else {
      pfbs[i].fontname = NEW (strlen(short_fontname)+1, char);
      strcpy (pfbs[i].fontname, short_fontname);
    }
    if (short_fontname)
      RELEASE (short_fontname);
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

static void do_pfb (int pfb_id)
{
  char *full_pfb_name;
  FILE *type1_binary_file;
  pdf_obj *stream_dict;
  unsigned long length1, length2, length3;
  int ch;
  full_pfb_name = kpse_find_file (pfbs[pfb_id].pfb_name, kpse_type1_format,
				  1);
  if (verbose) {
    fprintf (stderr, "(%s)", full_pfb_name);
  }
  if (full_pfb_name == NULL ||
      (type1_binary_file = FOPEN (full_pfb_name, FOPEN_RBIN_MODE)) == NULL) {
    fprintf (stderr, "type1_fontfile:  Unable to find or open binary font file (%s)",
	     pfbs[pfb_id].pfb_name);
    ERROR ("This existed when I checked it earlier!");
    return;
  }
  /* Following section doesn't hide PDF stream structure very well */
  if (!partial_enabled) {
    length1 = do_pfb_header (type1_binary_file, pfb_id, NULL);
    length2 = do_pfb_body (type1_binary_file, pfb_id, NULL);
  }
  else if (partial_enabled && pfbs[pfb_id].encoding_id >= 0) {
    length1 = do_pfb_header (type1_binary_file, pfb_id,
			     NULL);
    length2 = do_pfb_body (type1_binary_file, pfb_id,
			   (encodings[pfbs[pfb_id].encoding_id]).glyphs);
  }
  else if (partial_enabled && pfbs[pfb_id].encoding_id < 0) {
    char **glyphs = NEW (256, char *);
    length1 = do_pfb_header (type1_binary_file, pfb_id,
			     glyphs);
    length2 = do_pfb_body (type1_binary_file, pfb_id,
			   glyphs);
    release_glyphs (glyphs);
    RELEASE (glyphs);
  }
  length3 = do_pfb_trailer (type1_binary_file, pfbs[pfb_id].direct);
  if ((ch = fgetc (type1_binary_file)) != 128 ||
      (ch = fgetc (type1_binary_file)) != 3)
    ERROR ("type1_fontfile:  Are you sure this is a pfb?");
  /* Got entire file! */
  FCLOSE (type1_binary_file);
  stream_dict = pdf_stream_dict (pfbs[pfb_id].direct);
  pdf_add_dict (stream_dict, pdf_new_name("Length1"),
		pdf_new_number (length1));
  pdf_add_dict (stream_dict, pdf_new_name("Length2"),
		pdf_new_number (length2));
  pdf_add_dict (stream_dict, pdf_new_name("Length3"),
		pdf_new_number (length3));
  if (verbose > 1)
    fprintf (stderr, "Embedded size: %ld bytes\n", length1+length2+length3);
  return;
}


#define FIXED_WIDTH 1
#define SERIF 2
#define STANDARD 32
#define ITALIC 64
#define SYMBOLIC 4   /* Fonts that don't have Adobe encodings (e.g.,
			cmr, should be set to be symbolic */
#define STEMV 80

static pdf_obj *type1_font_descriptor (int encoding_id, int pfb_id, int tfm_font_id)
{
  pdf_obj *font_descriptor, *font_descriptor_ref, *tmp1;
  int flags;
  double italic_angle;
  font_descriptor = pdf_new_dict ();
  pdf_add_dict (font_descriptor,
		pdf_new_name ("Type"),
		pdf_new_name ("FontDescriptor"));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("CapHeight"),
		pdf_new_number (ROUND(1000.0*tfm_get_height(tfm_font_id,
						     'M'), 0.01)));
  {
    double max_height, max_depth, max_width;
    max_height = tfm_get_max_height (tfm_font_id)*1000.0;
    max_depth = tfm_get_max_depth (tfm_font_id)*1000.0;
    max_width = tfm_get_max_width (tfm_font_id)*1000.0;
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("Ascent"),
		  pdf_new_number (ROUND(max_height,0.01)));
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("Descent"),
		  pdf_new_number (ROUND(-max_depth,0.01)));
    tmp1 = pdf_new_array ();

    pdf_add_array (tmp1, pdf_new_number(ROUND(-0.10*max_width,1.0)));
    pdf_add_array (tmp1,
		   pdf_new_number(ROUND(-max_depth-0.10*(max_height+max_depth),1.0)));
    pdf_add_array (tmp1,
		   pdf_new_number(ROUND(MAX(1.10*max_width,1.0),1.0)));
    pdf_add_array (tmp1,
		   pdf_new_number(ROUND(MAX(max_height+0.10*(max_height+max_depth),1.0),1.0)));
    pdf_add_dict (font_descriptor, pdf_new_name ("FontBBox"), tmp1);
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("FontName"),
		  pdf_new_name (type1_fontname(pfb_id)));
  }
#ifndef M_PI
  #define M_PI (4.0*atan(1.0))
#endif
  italic_angle = ROUND(-180.0/M_PI*atan(tfm_get_it_slant(tfm_font_id)),0.1);
  pdf_add_dict (font_descriptor,
		pdf_new_name ("ItalicAngle"),
		pdf_new_number(italic_angle));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("XHeight"),
		pdf_new_number
		(ROUND(tfm_get_x_height(tfm_font_id)*1000.0,0.1)));
  pdf_add_dict (font_descriptor,
		pdf_new_name ("StemV"),  /* This is required */
		pdf_new_number (STEMV));
  /* You don't need a fontfile for the standard fonts */
  if (pfb_id >= 0)
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("FontFile"),
		  type1_fontfile (pfb_id));
  font_descriptor_ref = pdf_ref_obj (font_descriptor);

  /* Take care of flags */
  {
    flags = 0;
    if (italic_angle != 0.0)
      flags += ITALIC;
    if (tfm_is_fixed_width(tfm_font_id))
      flags += FIXED_WIDTH;
    flags += SYMBOLIC;
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("Flags"),
		  pdf_new_number (flags));
  }
  pdf_release_obj (font_descriptor);
  return font_descriptor_ref;
}


struct a_type1_font
{
  pdf_obj *indirect;
  int pfb_id;
  double slant, extend;
  int remap;
} *type1_fonts = NULL;
int num_type1_fonts = 0, max_type1_fonts = 0;

pdf_obj *type1_font_resource (int type1_id)
{
  if (type1_id>=0 && type1_id<max_type1_fonts)
    return pdf_link_obj(type1_fonts[type1_id].indirect);
  else {
    ERROR ("Invalid font id in type1_font_resource");
    return NULL;
  }
}

double type1_font_slant (int type1_id)
{
  if (type1_id>=0 && type1_id<max_type1_fonts)
    return type1_fonts[type1_id].slant;
  else {
    ERROR ("Invalid font id in type1_font_slant");
    return 0.0;
  }
}

double type1_font_extend (int type1_id)
{
  if (type1_id>=0 && type1_id<max_type1_fonts)
    return type1_fonts[type1_id].extend;
  else {
    ERROR ("Invalid font id in type1_font_extend");
    return 1.0;
  }
}

int type1_font_remap (int type1_id)
{
  if (type1_id>=0 && type1_id<max_type1_fonts)
    return (partial_enabled && type1_fonts[type1_id].remap);
  else {
    ERROR ("Invalid font id in type1_font_remap");
    return 0;
  }
}

char *type1_font_used (int type1_id)
{
  int pfb_id;
  char *result;
  if (type1_id>=0 && type1_id<max_type1_fonts &&
      (pfb_id = type1_fonts[type1_id].pfb_id) >= 0 &&
      (pfb_id <max_pfbs))
    result = pfbs[pfb_id].used_chars;
  else if (type1_id >= 0 && type1_id < max_type1_fonts){
    result = NULL;
  } else {
    fprintf (stderr, "type1_font_used: type1_id=%d\n", type1_id);
    ERROR ("Invalid font id in type1_font_used");
  }
  return result;
}


static int is_a_base_font (char *name)
{
  static char *basefonts[] = {
    "Courier",			"Courier-Bold",		"Courier-Oblique",
    "Courier-BoldOblique",	"Helvetica",		"Helvetica-Bold",
    "Helvetica-Oblique",	"Helvetica-BoldOblique",	"Symbol",
    "Times-Roman",		"Times-Bold",		"Times-Italic",
    "Times-BoldItalic",		"ZapfDingbats"
  };
  int i;
  for (i=0; i<14; i++) {
    if (!strcmp (name, basefonts[i]))
      return 1;
  }
  return 0;
}

int type1_font (const char *tex_name, int tfm_font_id, char *resource_name)
{
  int i, result = -1;
  int tfm_firstchar, tfm_lastchar;
  int pdf_firstchar, pdf_lastchar;
  int encoding_id = -1;
  int pfb_id = -1;
  pdf_obj *font_resource, *tmp1, *font_encoding_ref;
  struct font_record *font_record;
  /* If we are at the limit, Make sure we have storage in case we end
     up using a new font */
  if (num_type1_fonts >= max_type1_fonts) {
    max_type1_fonts += MAX_FONTS;
    type1_fonts = RENEW (type1_fonts, max_type1_fonts, struct a_type1_font);
  }
  font_record = get_font_record (tex_name);
  /* Fill in default value for font_name and enc if not specified in map file */
  if (verbose>1){
    if (font_record) {
      fprintf (stderr, "\nfontmap: %s -> %s", tex_name,
	       font_record->font_name);
      if (font_record->enc_name)
	fprintf (stderr, "(%s)", font_record->enc_name);
      if (font_record->slant)
	fprintf (stderr, "[slant=%g]", font_record->slant);
      if (font_record->extend != 1.0)
	fprintf (stderr, "[extend=%g]", font_record->extend);
      if (font_record->remap)
	fprintf (stderr, "[remap]");
      fprintf (stderr, "\n");
    } else {
      fprintf (stderr, "\nfontmap: %s (no map)\n", tex_name);
    }
  }
  if (font_record && font_record -> enc_name != NULL) {
    encoding_id = get_encoding (font_record -> enc_name);
  }
  if ((font_record && is_a_base_font(font_record->font_name)) ||
      (pfb_id = type1_pfb_id(font_record? font_record -> font_name: tex_name,
			     encoding_id,
			     font_record? font_record -> remap: 0)) >= 0) {
    /* Looks like we have a physical font (either a reader font or a
       Type 1 font binary file).  */
    type1_fonts[num_type1_fonts].pfb_id = pfb_id;
    type1_fonts[num_type1_fonts].extend = font_record? font_record -> extend: 1.0;
    type1_fonts[num_type1_fonts].slant = font_record? font_record -> slant: 0.0;
    type1_fonts[num_type1_fonts].remap = font_record? font_record -> remap: 0;
    /* Allocate a dictionary for the physical font */
    font_resource = pdf_new_dict ();
    if (encoding_id >= 0) {
      font_encoding_ref = pdf_link_obj(encodings[encoding_id].encoding_ref);
      pdf_add_dict (font_resource,
		    pdf_new_name ("Encoding"),
		    font_encoding_ref);
    }
    pdf_add_dict (font_resource,
		  pdf_new_name ("Type"),
		  pdf_new_name ("Font"));
    pdf_add_dict (font_resource,
		  pdf_new_name ("Subtype"),
		  pdf_new_name ("Type1"));
    pdf_add_dict (font_resource, 
		  pdf_new_name ("Name"),
		  pdf_new_name (resource_name));
    if (type1_fonts[num_type1_fonts].pfb_id >= 0) {
      pdf_add_dict (font_resource, 
		    pdf_new_name ("FontDescriptor"),
		    type1_font_descriptor(encoding_id,
					  type1_fonts[num_type1_fonts].pfb_id,
					  tfm_font_id));
    }
      /* If we are embedding this font, it may have been used by another virtual
	 font and we need to use the same mangled name.  Mangled
	 names are known only to the pfb module, so we call it to get
	 the name */
    if (type1_fonts[num_type1_fonts].pfb_id >= 0) {
      pdf_add_dict (font_resource, 
		    pdf_new_name ("BaseFont"),
		    pdf_new_name
		    (type1_fontname(type1_fonts[num_type1_fonts].pfb_id)));
	/* Otherwise we use the base name */
    } else {
      pdf_add_dict (font_resource,
		    pdf_new_name ("BaseFont"),
		    pdf_new_name (font_record?font_record->font_name:tex_name));
    }
    if (!(font_record && is_a_base_font (font_record->font_name))) {
      tfm_firstchar = tfm_get_firstchar(tfm_font_id);
      tfm_lastchar = tfm_get_lastchar(tfm_font_id);
      if (partial_enabled && type1_fonts[num_type1_fonts].remap) {
	unsigned char t;
	pdf_firstchar=255; pdf_lastchar=0;
	for (i=tfm_firstchar; i<=tfm_lastchar; i++) {
	  if ((t=twiddle(i)) < pdf_firstchar)
	    pdf_firstchar = t;
	  if (t > pdf_lastchar)
	    pdf_lastchar = t;
	}
      } else {
	pdf_firstchar = tfm_firstchar;
	pdf_lastchar = tfm_lastchar;
      }
      pdf_add_dict (font_resource,
		    pdf_new_name ("FirstChar"),
		    pdf_new_number (pdf_firstchar));
      pdf_add_dict (font_resource,
		    pdf_new_name ("LastChar"),
		    pdf_new_number (pdf_lastchar));
      tmp1 = pdf_new_array ();
      for (i=pdf_firstchar; i<=pdf_lastchar; i++) {
	if (partial_enabled && type1_fonts[num_type1_fonts].remap) {
	  int t;
	  if ((t=twiddle(i)) <= tfm_lastchar && t>=tfm_firstchar)
	    pdf_add_array (tmp1,
			   pdf_new_number(ROUND(tfm_get_width
						(tfm_font_id,t)*1000.0,0.01)));
	  else
	    pdf_add_array (tmp1,
			   pdf_new_number(0.0));
	} else
	  pdf_add_array (tmp1,
			 pdf_new_number(ROUND(tfm_get_width
					      (tfm_font_id, i)*1000.0,0.01)));
      }
      pdf_add_dict (font_resource,
		    pdf_new_name ("Widths"),
		    tmp1);
    }
    type1_fonts[num_type1_fonts].indirect = pdf_ref_obj(font_resource);
    pdf_release_obj (font_resource);
    result = num_type1_fonts;
    num_type1_fonts += 1;
  } else { /* Don't have a physical font */
    result = -1;
  }
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
    RELEASE (pfbs[i].used_chars);
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
  for (i=0; i<num_font_map; i++) {
    if (font_map[i].tex_name)
      RELEASE (font_map[i].tex_name);
    if (font_map[i].enc_name)
      RELEASE (font_map[i].enc_name);
    if (font_map[i].font_name)
      RELEASE (font_map[i].font_name);
  }
  if (font_map)
    RELEASE (font_map);
}

