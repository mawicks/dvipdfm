/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/type1.c,v 1.45 1998/12/26 04:53:34 mwicks Exp $

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

/* This is tailored for PDF */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <kpathsea/tex-file.h>
#include <time.h>
#include <ctype.h>
#include "system.h"
#include "pdfobj.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"
#include "numbers.h"
#include "type1.h"
#include "tfm.h"
#include "pdfparse.h"
#include "pdflimits.h"
#include "t1crypt.h"

static const char *map_filename = "pdffonts.map";

struct font_record 
{
  char *afm_name;
  char *pfb_name;
  char *enc_name;
};

struct encoding {
  char *enc_name;
  /* The following array isn't very efficient. It is constructed
     by peeling the names from the encoding object.  It makes
     it easier to construct an array in this format when the
     encoding must be obtained directly from the PFB file */
  char *glyphs[256];
  pdf_obj *encoding_ref;
} encodings[MAX_ENCODINGS];
int num_encodings = 0;

void type1_set_mapfile (const char *name)
{
  map_filename = name;
  return;
}

#include "winansi.h"

pdf_obj *find_encoding_differences (pdf_obj *encoding)
{
  static filling;
  int i;
  pdf_obj *result = pdf_new_array ();
  pdf_obj *tmp;
  filling = 0;
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
  if ((full_enc_filename = kpse_find_file (enc_name,
					   kpse_tex_ps_header_format,
					   1)) == NULL) {
    strcpy (tmp = NEW (strlen(enc_name)+5, char), enc_name);
    strcat (tmp, ".enc");
    full_enc_filename = kpse_find_file (tmp,
					kpse_tex_ps_header_format,
					1);
    RELEASE (tmp);
  }
  if (full_enc_filename == NULL ||
      (encfile = fopen (full_enc_filename, "r")) == NULL ||
      (filesize = file_size (encfile)) == 0) {
    if (encfile)
      fclose (encfile);
    sprintf (work_buffer, "Can't find or open encoding file: %s", enc_name) ;
    ERROR (work_buffer);
  }
  /* Got one and opened it */
  {
    char *buffer, *start, *end, *junk_ident;
    pdf_obj *junk_obj, *encoding, *differences;
    buffer = NEW (filesize, char); 
    fread (buffer, sizeof (char), filesize, encfile);
    fclose (encfile);
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
    differences = find_encoding_differences (encoding);
    /* Put the glyph names into a conventional array */
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

struct font_record *get_font_record (const char *tex_name)
{
  struct font_record *result;
  static first = 1;
  static FILE *mapfile;
  char *full_map_filename, *start, *end = NULL, *record_name;
  result = NEW (1, struct font_record);
  result -> enc_name = NULL;
  result -> afm_name = NULL;
  result -> pfb_name = NULL;
  
  if (first) {
    first = 0;
    full_map_filename = kpse_find_file (map_filename, kpse_tex_ps_header_format,
					0);
    if (full_map_filename == NULL || 
	(mapfile = fopen (full_map_filename, "r")) == NULL) {
      fprintf (stderr, "Warning:  No font map file\n");
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
  result -> afm_name = parse_ident (&start, end); /* May be null */
  skip_white (&start, end);
  result -> pfb_name = parse_ident (&start, end); /* May be null */
  return result;
}




static unsigned long get_low_endian_quad (FILE *file)
{
  unsigned long result;
  unsigned bytes[4];
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
static num_pfbs = 0;
static max_pfbs = 0;
struct a_pfb
{
  char *pfb_name;
  char *fontname;
  pdf_obj *direct, *indirect;
  char used_chars[256];
  int encoding_id;
} *pfbs = NULL;

static void clear_a_pfb (struct a_pfb *pfb)
{
  int i;
  for (i=0; i<256; i++) {
    (pfb->used_chars)[i] = 0;
  }
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
      case '[':
      case ']':  
      case '{':
      case '}':
	start += 1;
	break;
      case '(':
	pdfobj = parse_pdf_string (&start, end);
	if (pdfobj == NULL) {
	  ERROR ("parse_header:  Error parsing pfb header");
	}
	pdf_release_obj (pdfobj);
	if (state >= 1)
	  state = 1;
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
      if (ident != NULL && !strcmp (ident, "put") && 
	  (int) last_number < 256 && (int) last_number >= 0 && glyphs) {
	if (glyphs[last_number] != NULL) 
	  RELEASE (glyphs[last_number]);
	glyphs[last_number] = glyph;
	if ((pfbs[pfb_id].used_chars)[last_number]) {
	  lead = saved_lead;
	}
      } else {
	RELEASE (glyph);
	lead = saved_lead;
      }
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

static unsigned int glyph_length (char **glyphs) 
{
  int i;
  unsigned result = 0;
  for (i=0; i<256; i++) {
    result += strlen (glyphs[i]);
  }
  return result;
}


static unsigned long do_pfb_header (FILE *file, int pfb_id,
				    char **glyphs)
{
  int i, ch;
  int stream_type;
  unsigned char *buffer, *filtered;
  unsigned long length, nread;
  if ((ch = fgetc (file)) < 0 || ch != 128){
    fprintf (stderr, "Got %d, expecting 128\n", ch);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  if ((stream_type = fgetc (file)) < 0 || stream_type != ASCII) {
    fprintf (stderr, "Got %d, expecting %d\n", ch, ASCII);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  length = get_low_endian_quad (file);
  buffer = NEW (length, unsigned char);
  if ((nread = fread(buffer, sizeof(unsigned char), length, file)) ==
      length) {
    for (i=0; i<nread; i++) {
      if (buffer[i] == '\r')
	buffer[i] = '\n';  /* May not be portable to non-Unix
			      systems */
    }
    if (partial_enabled) {
      filtered = NEW (length+strlen(pfbs[pfb_id].fontname)+1, unsigned char);
      length = parse_header (filtered, buffer, length, pfb_id, glyphs);
      pdf_add_stream (pfbs[pfb_id].direct, (char *) filtered, length);
      RELEASE (filtered);
    } else {
      pdf_add_stream (pfbs[pfb_id].direct, (char *) buffer, length);
    }
  } else {
    fprintf (stderr, "Found only %ld out of %ld bytes\n", nread, length);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  RELEASE (buffer);
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
      pdf_release_obj (pdfobj);
      continue;
    case '/':
      start += 1;
      ident = parse_ident (&start, end);
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
    fprintf (stderr, "\nEmbedding %d glyphs\n", nused);
    filtered_pointer += sprintf ((char *) filtered_pointer, " %d",
				 nused);
    skip_white(&start, end);
    /* The following ident *should* be the number of glyphs in this
       file */
    ident = parse_ident (&start, end);
    if (ident == NULL || !is_a_number (ident) || nused > atof (ident)) 
      ERROR ("More glyphs needed than present in file");
    fprintf (stderr, "File has %s glyphs\n", ident);
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
      ident = parse_ident (&start, end);
      if (!is_a_number (ident))
	ERROR ("Expecting a number after glyph name");
      last_number = atof (ident);
      RELEASE (ident);
      /* The next identifier should be a "RD" or a "-|".  We don't
	 really care what it is */
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
    if (start >= end) {
      ERROR ("Premature end of glyph definitions in font file");
    }
    if (nleft != 0)
      ERROR ("Didn't find all the required glyphs in the font");
    /* Include the rest of the file verbatim */
    memcpy (filtered_pointer, start, end-start);
    filtered_pointer += end-start;
  }
  return (filtered_pointer-filtered);
}



static unsigned long do_pfb_body (FILE *file, int pfb_id,
				  char **glyphs)
{
  int i, ch;
  int stream_type;
  unsigned char *buffer, *filtered;
  unsigned long length, nread;
  if ((ch = fgetc (file)) < 0 || ch != 128){
    fprintf (stderr, "Got %d, expecting 128\n", ch);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  if ((stream_type = fgetc (file)) < 0 || stream_type != BINARY) {
    fprintf (stderr, "Got %d, expecting %d\n", ch, BINARY);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  length = get_low_endian_quad (file);
  buffer = NEW (length, unsigned char);
  filtered = NEW (length, unsigned char);
  if ((nread = fread(buffer, sizeof(unsigned char), length, file)) ==
      length) {
    if (partial_enabled && glyphs != NULL && partial_enabled) {
      /* For partial font embedding we need to decrypt the binary
	 portin of the pfb */
      crypt_init(EEKEY);
      for (i=0; i<nread; i++) {
	buffer[i] = decrypt(buffer[i]);
      }
      /* Now we do the partial embedding */
      length = do_partial_body (filtered, buffer, length, 
				pfbs[pfb_id].used_chars,
				glyphs);
      /* And reencrypt the whole thing */
      crypt_init (EEKEY);
      for (i=0; i<length; i++) {
	buffer[i] = encrypt(filtered[i]);
      }
    }
    pdf_add_stream (pfbs[pfb_id].direct, (char *) buffer, length);
  } else {
    fprintf (stderr, "Found only %ld/%ld bytes\n", nread, length);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  RELEASE (buffer);
  RELEASE (filtered);
  return length;
}

static unsigned long do_pfb_trailer (FILE *file, pdf_obj *stream)
{
  int i, ch;
  int stream_type;
  unsigned char *buffer;
  unsigned long length, nread;
  if ((ch = fgetc (file)) < 0 || ch != 128){
    fprintf (stderr, "Got %d, expecting 128\n", ch);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  if ((stream_type = fgetc (file)) < 0 || stream_type != ASCII) {
    fprintf (stderr, "Got %d, expecting %d\n", ch, ASCII);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  length = get_low_endian_quad (file);
  buffer = NEW (length, unsigned char);
  if ((nread = fread(buffer, sizeof(unsigned char), length, file)) ==
      length) {
    for (i=0; i<nread; i++) {
      if (buffer[i] == '\r')
	buffer[i] = '\n';  /* May not be portable to non-Unix
			      systems */
    }
    pdf_add_stream (stream, (char *) buffer, nread);
  } else {
    fprintf (stderr, "Found only %ld out of %ld bytes\n", nread, length);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
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

static int type1_pfb_id (const char *pfb_name, int encoding_id, char *fontname)
{
  int i;
  for (i=0; i<num_pfbs; i++) {
    if (pfbs[i].pfb_name && !strcmp (pfbs[i].pfb_name, pfb_name))
      break;
  }
  if (i == num_pfbs) {
    char *full_pfb_name;
    full_pfb_name = kpse_find_file (pfb_name, kpse_type1_format,
				    1);
    if (full_pfb_name == NULL) {
      fprintf (stderr, "type1_fontfile:  Unable to find binary font file (%s)...Hope that's okay.", pfb_name);
      return -1;
    }
    if (num_pfbs >= max_pfbs) {
      max_pfbs += MAX_FONTS;
      pfbs = RENEW (pfbs, max_pfbs, struct a_pfb);
    }
    num_pfbs += 1;
    clear_a_pfb (pfbs+i);
    pfbs[i].pfb_name = NEW (strlen(pfb_name)+1, char);
    strcpy (pfbs[i].pfb_name, pfb_name);
    pfbs[i].direct = pdf_new_stream(STREAM_COMPRESS);
    pfbs[i].indirect = pdf_ref_obj (pfbs[i].direct);
    pfbs[i].encoding_id = encoding_id;
    pfbs[i].fontname = NEW (strlen(fontname)+1, char);
    strcpy (pfbs[i].fontname, fontname);
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
  if (full_pfb_name == NULL ||
      (type1_binary_file = fopen (full_pfb_name, FOPEN_RBIN_MODE)) == NULL) {
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
  fclose (type1_binary_file);
  stream_dict = pdf_stream_dict (pfbs[pfb_id].direct);
  pdf_add_dict (stream_dict, pdf_new_name("Length1"),
		pdf_new_number (length1));
  pdf_add_dict (stream_dict, pdf_new_name("Length2"),
		pdf_new_number (length2));
  pdf_add_dict (stream_dict, pdf_new_name("Length3"),
		pdf_new_number (length3));
  return;
}


#define FONTNAME 1
#define CAPHEIGHT 2
#define ASCENT   3
#define DESCENT  4
#define ISFIXED  5
#define FONTBBOX 6
#define ITALICANGLE 7
#define XHEIGHT  8  
#define COMMENT  9
#define SFONTMETRICS 10
#define EFONTMETRICS 11
#define SCHARMETRICS 12
#define ECHARMETRICS 13
#define CHARDEF      14
#define OTHER        99

static struct {
  char *string, value;
} parse_table[] = 
{  {"FontName", FONTNAME },  { "CapHeight", CAPHEIGHT },
   {"Ascender", ASCENT },  { "Descender", DESCENT },
   {"IsFixedPitch", ISFIXED },
   {"FontBBox", FONTBBOX },  { "ItalicAngle", ITALICANGLE },
   {"XHeight", XHEIGHT },  { "Comment", COMMENT },
   {"StartFontMetrics", SFONTMETRICS },  { "EndFontMetrics", EFONTMETRICS },
   {"StartCharMetrics", SCHARMETRICS },  { "EndCharMetrics", ECHARMETRICS },
};


static char buffer[256];
static char *start;
static char *end;
static FILE *type1_afm_file;

static int get_afm_token (void)
{
  int i;
  if (mfgets (buffer, sizeof(buffer), type1_afm_file)) {
    if (strlen (buffer) == sizeof(buffer)-1) {
      ERROR ("get_afm_token:  Line to long");
    }
    end = buffer + sizeof(buffer);
    for (i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); i++) {
      if (!strncmp(buffer, parse_table[i].string, strlen(parse_table[i].string))){
	start = buffer + strlen(parse_table[i].string) + 1;
	return parse_table[i].value;
      }
    }
    return OTHER;
  }
  return (-1);
}

static double descent, ascent;
static double bbllx, bblly, bburx, bbury, xheight;
static double capheight, italicangle;
static int isfixed;
static char fontname[256];  /* Make as long as buffer */

static void reset_afm_variables (void)
{
  descent = 0.0; ascent = 0.0;
  bbllx = 0.0; bblly = 0.0;
  bburx = 0.0; bbury = 0.0;
  capheight = 0.0; italicangle = 0.0;
  isfixed = 0;
  return;
}

static int open_afm_file (const char *afm_name)
{
  static char *full_afm_name;
  reset_afm_variables ();
  full_afm_name = kpse_find_file (afm_name, kpse_afm_format,
				  1);
  if (full_afm_name &&
      (type1_afm_file = fopen (full_afm_name, FOPEN_R_MODE)) != NULL)
    return 1;
  else
    return 0;
}

static void close_afm_file (void)
{
  fclose (type1_afm_file);
  return;
}


static void scan_afm_file (void)
{
  int token;
  while ((token = get_afm_token()) >= 0) {
    skip_white (&start, end); 
    if (start < end)
    switch (token) {
    case FONTNAME:
      if (sscanf (start, " %s ", fontname) != 1)
	ERROR ("afm: Error reading Fontname");
      break;
    case CAPHEIGHT:
      if (sscanf (start, " %lf", &capheight) != 1)
	ERROR ("afm: Error reading Capheight");
      break;
    case ASCENT:
      if (sscanf (start, " %lf", &ascent) != 1)
	ERROR ("afm: Error reading ascent");
      break;
    case DESCENT:
      if (sscanf (start, " %lf", &descent) != 1)
	ERROR ("afm: Error reading descent");
      break;
    case ISFIXED:
      if (!strncmp(start, "false", 5))
	isfixed = 0;
      else if (!strncmp(start, "true", 4))
	isfixed = 1;
      else 
	ERROR ("Can't read value for IsFixedPitch");
      break;
    case FONTBBOX:
      if (sscanf (start, " %lf %lf %lf %lf ", &bbllx, &bblly, &bburx, &bbury) != 4)
	ERROR ("afm: Error reading FontBBox");
      break;
    case ITALICANGLE:
      if (sscanf (start, " %lf", &italicangle) != 1)
	ERROR ("afm: Error reading descent");
      break;
    case XHEIGHT:
      if (sscanf (start, " %lf", &xheight) != 1)
	ERROR ("afm: Error reading XHeight");
      break;
    case COMMENT:
    case SFONTMETRICS:
    case EFONTMETRICS:
    case ECHARMETRICS:
    default:
      break;
    }
  }
  return;
}

#define FIXED_WIDTH 1
#define SERIF 2
#define STANDARD 32
#define ITALIC 64
#define SYMBOLIC 4   /* Fonts that don't have Adobe encodings (e.g.,
			cmr, should be set to be symbolic */
#define NOCLUE 20

static pdf_obj *type1_font_descriptor (const char *pfb_name, int encoding_id,
				int pfb_id)
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
  flags += STANDARD;
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
		pdf_new_number (NOCLUE));
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
  if (font_record -> afm_name != NULL && 
      (!strcmp (font_record->afm_name, "default") ||
       !strcmp (font_record->afm_name, "none"))) {
    RELEASE(font_record->afm_name);
    font_record -> afm_name = NULL;
  }
  /* We *must* fill in an afm_name either explicitly or by default */
  if (font_record -> afm_name == NULL) {
    font_record -> afm_name = NEW (strlen(tex_name)+1, char);
    strcpy (font_record->afm_name, tex_name);
  }
  /* If a pfb_name wasn't specified, default to afm_name */
  if (font_record -> pfb_name == NULL) {
    font_record -> pfb_name = NEW (strlen(font_record->afm_name)+1, char);
    strcpy (font_record->pfb_name, font_record->afm_name);
  }
  /* If a "none" pfb name was specified, set name to null */
  if (font_record -> pfb_name != NULL && 
      !strcmp (font_record->pfb_name, "none")) {
    RELEASE(font_record->pfb_name);
    font_record -> pfb_name = NULL;
  }
  return;
}

struct a_type1_font
{
  pdf_obj *indirect;
  int pfb_id;
} *type1_fonts;
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

static void mangle_fontname()
{
  int i;
  char ch;
  static first = 1;
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

int type1_font (const char *tex_name, int tfm_font_id, const char *resource_name)
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
	  if (partial_enabled) {
	    mangle_fontname();
	  }
	  type1_fonts[num_type1_fonts].pfb_id =
	    type1_pfb_id (font_record -> pfb_name, encoding_id, fontname);
	  pdf_add_dict (font_resource, 
			pdf_new_name ("FontDescriptor"),
			type1_font_descriptor(font_record -> pfb_name,
					      encoding_id,
					      type1_fonts[num_type1_fonts].pfb_id));
	}
      }
      pdf_add_dict (font_resource,
		    pdf_new_name ("BaseFont"),
		    pdf_new_name (fontname));  /* fontname is global and set
						  by scan_afm_file() */
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
}
