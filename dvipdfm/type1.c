/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/type1.c,v 1.10 1998/12/05 11:47:25 mwicks Exp $

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
#include <math.h>
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

static const char *map_filename = "pdffonts.map";

struct font_record 
{
  char *afm_name;
  char *pfb_name;
  char *enc_name;
};

struct encoding {
  char *enc_name;
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
    /* Boy do I regret starting arrays at 1 */
    tmp = pdf_get_array (encoding, i+1);
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

pdf_obj *get_encoding (const char *enc_name)
{
  FILE *encfile = NULL;
  char *full_enc_filename, *tmp;
  pdf_obj *result, *result_ref;
  long filesize;
  int i;
  /* See if we already have this saved */
  for (i=0; i<num_encodings; i++)
    if (!strcmp (enc_name, encodings[i].enc_name))
      return pdf_link_obj (encodings[i].encoding_ref);
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
    differences = find_encoding_differences (encoding);
    pdf_release_obj (encoding);
    result = pdf_new_dict();
    pdf_add_dict (result, pdf_new_name ("Type"),
		  pdf_new_name ("Encoding"));
    pdf_add_dict (result, pdf_new_name ("BaseEncoding"),
		  pdf_new_name ("WinAnsiEncoding"));
    pdf_add_dict (result, pdf_new_name ("Differences"),
		  differences);
  }
  result_ref = pdf_ref_obj (result);
  pdf_release_obj (result);

  encodings[num_encodings].encoding_ref = result_ref;
  encodings[num_encodings].enc_name = NEW (strlen(enc_name)+1, char);
  strcpy (encodings[num_encodings].enc_name, enc_name);
  num_encodings += 1;

  return pdf_link_obj(result_ref);
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
    if (strcmp (record_name, tex_name)) {
      RELEASE (record_name);
      continue;
    }
    break;
  }
  if (start == NULL)
    return result;
  result = NEW (1, struct font_record);
  skip_white (&start, end);
  result -> enc_name = parse_ident (&start, end); /* May be null */  
  skip_white (&start, end);
  result -> afm_name = parse_ident (&start, end); /* May be null */
  skip_white (&start, end);
  result -> pfb_name = parse_ident (&start, end); /* May be null */
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

#define ASCII 1
#define BINARY 2

static unsigned long do_pfb_segment (FILE *file, int expected_type, pdf_obj *stream)
{
  int i, ch, ntotal = 0;
  int stream_type;
  unsigned long length;
  if ((ch = fgetc (file)) < 0 || ch != 128){
    fprintf (stderr, "Got %d, expecting 128\n", ch);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  if ((stream_type = fgetc (file)) < 0 || stream_type != expected_type ) {
    fprintf (stderr, "Got %d, expecting %d\n", ch, expected_type);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  length = get_low_endian_quad (file);
  while (ntotal < length) {
    int nread, nleft, n_to_read;
    nleft = length - ntotal;
    n_to_read = (nleft < WORK_BUFFER_SIZE)? nleft: WORK_BUFFER_SIZE;
    if ((nread = fread (work_buffer, sizeof(char), 
			n_to_read, file)) > 0) {

      for (i=0; i<nread; i++) {
	if (work_buffer[i] == '\r' && stream_type == ASCII)
	  work_buffer[i] = '\n';  /* May not be portable to non-Unix
				     systems */
      }
      pdf_add_stream (stream, work_buffer, nread);
      ntotal += nread;
    } else{
      fprintf (stderr, "Found only %d bytes\n", ntotal);
      ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
    }
  }
  return length;
}

pdf_obj *type1_fontfile (const char *pfb_name)
{
  FILE *type1_binary_file;
  pdf_obj *stream, *stream_dict, *stream_label;
  unsigned long length1, length2, length3;
  char *full_pfb_name;
  int ch;
  full_pfb_name = kpse_find_file (pfb_name, kpse_type1_format,
				  1);
  if (full_pfb_name == NULL ||
      (type1_binary_file = fopen (full_pfb_name, FOPEN_RBIN_MODE)) == NULL) {
    fprintf (stderr, "type1_fontfile:  Unable to find or open binary font file (%s)...Hope that's okay.", pfb_name);
    return NULL;
  }
  stream = pdf_new_stream();
  /* Following line doesn't hide PDF stream structure very well */
  length1 = do_pfb_segment (type1_binary_file, ASCII, stream);
  length2 = do_pfb_segment (type1_binary_file, BINARY, stream);
  length3 = do_pfb_segment (type1_binary_file, ASCII, stream);
  if ((ch = fgetc (type1_binary_file)) != 128 ||
      (ch = fgetc (type1_binary_file)) != 3)
    ERROR ("type1_fontfile:  Are you sure this is a pfb?");
  /* Got entire file! */
  fclose (type1_binary_file);
  stream_dict = pdf_stream_dict (stream);
  pdf_add_dict (stream_dict, pdf_new_name("Length1"),
		pdf_new_number (length1));
  pdf_add_dict (stream_dict, pdf_new_name("Length2"),
		pdf_new_number (length2));
  pdf_add_dict (stream_dict, pdf_new_name("Length3"),
		pdf_new_number (length3));
  stream_label = pdf_ref_obj (stream);
  pdf_release_obj (stream);
  return stream_label;
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

static void open_afm_file (const char *afm_name)
{
  static char *full_afm_name;
  reset_afm_variables ();
  full_afm_name = kpse_find_file (afm_name, kpse_afm_format,
				  1);
  if (full_afm_name == NULL || 
      (type1_afm_file = fopen (full_afm_name, FOPEN_R_MODE)) == NULL) {
    sprintf (work_buffer, "open_afm_file: Unable to find or open AFM file named (%s)", afm_name);
    ERROR (work_buffer);
  }
  return;
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
#define ITALIC 64
#define SYMBOLIC 4   /* Fonts that don't have Adobe encodings (e.g.,
			cmr, should be set to be symbolic */
#define NOCLUE 20

pdf_obj *type1_font_descriptor (const char *pfb_name)
{
  pdf_obj *font_descriptor, *font_descriptor_ref, *fontfile, *tmp1;
  int flags;
  font_descriptor = pdf_new_dict ();
  pdf_add_dict (font_descriptor,
		pdf_new_name ("Type"),
		pdf_new_name ("FontDescriptor"));
  if (capheight == 0.0) {
    fprintf (stderr, "\nWarning:  type1_font_descriptor:  AFM file: CapHeight is zero\n");
  }
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
		pdf_new_number (NOCLUE));
  /* You don't need a fontfile for the standard fonts */
  if ((fontfile = type1_fontfile (pfb_name)) != NULL) 
    pdf_add_dict (font_descriptor,
		  pdf_new_name ("FontFile"),
		  fontfile);
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


pdf_obj *type1_font_resource (const char *tex_name, int tfm_font_id, const char *resource_name)
{
  int i;
  int firstchar, lastchar;
  pdf_obj *font_resource, *font_resource_ref, *tmp1;
  pdf_obj *font_encoding_ref;
  struct font_record *font_record;
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
  /* font_record should always result non-null value with default
     values filled in */
  font_record = get_font_record (tex_name);
  fill_in_defaults (font_record, tex_name);
  {
    open_afm_file (font_record ->afm_name);
    scan_afm_file();
    close_afm_file ();
    RELEASE (font_record -> afm_name);
  }
  if (font_record -> pfb_name != NULL) {
    if (!is_a_base_font (fontname))
      pdf_add_dict (font_resource, 
		    pdf_new_name ("FontDescriptor"),
		    type1_font_descriptor(font_record -> pfb_name));
    RELEASE (font_record -> pfb_name);
  }
  pdf_add_dict (font_resource,
		pdf_new_name ("BaseFont"),
		pdf_new_name (fontname));  /* fontname is set
						     by
						     type1_font_descriptor() */
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
  if (font_record -> enc_name != NULL) {
    font_encoding_ref = get_encoding (font_record -> enc_name);
    pdf_add_dict (font_resource,
		  pdf_new_name ("Encoding"),
		  font_encoding_ref);
    RELEASE (font_record -> enc_name);
  }
  RELEASE (font_record);
  font_resource_ref = pdf_ref_obj (font_resource);
  pdf_release_obj (font_resource);
  return font_resource_ref;
}

