/* This is tailored for PDF */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "pdfobj.h"
#include "mem.h"
#include "error.h"
#include "kpathsea/tex-file.h"
#include "io.h"
#include "numbers.h"
#include "type1.h"
#include "tfm.h" 

static is_a_base_font (char *name)
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
  static char buffer[256];
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

pdf_obj *type1_fontfile (const char *tex_name)
{
  FILE *type1_binary_file;
  pdf_obj *stream, *stream_dict, *stream_label, *tmp1, *tmp2;
  unsigned long length1, length2, length3;
  char *pfb_name, *full_pfb_name;
  int ch;
  pfb_name = NEW (strlen (tex_name) + 5, char);
  strcpy (pfb_name, tex_name);
  strcat (pfb_name, ".pfb");
  full_pfb_name = kpse_find_file (pfb_name, kpse_dvips_header_format,
				  1);
  if (full_pfb_name == NULL) {
    fprintf (stderr, "%s: ", pfb_name);
    ERROR ("type1_fontfile:  Unable to find binary font file");
  }
  fprintf (stderr, "(%s", pfb_name);
  release (pfb_name);
  if ((type1_binary_file = fopen (full_pfb_name, "rb")) == NULL) {
    fprintf (stderr, "type1_fontfile:  %s\n", pfb_name);
    ERROR ("type1_fontfile:  Unable to open binary font file");
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
  fprintf (stderr, ")");
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
static char *position;
static FILE *type1_afm_file;

static get_afm_token (void)
{
  int i;
  if (fgets (buffer, sizeof(buffer), type1_afm_file)) {
    if (strlen (buffer) == sizeof(buffer)-1) {
      ERROR ("get_afm_token:  Line to long");
    }
    for (i=0; i<sizeof(parse_table)/sizeof(parse_table[0]); i++) {
      if (!strncmp(buffer, parse_table[i].string, strlen(parse_table[i].string))){
	position = buffer + strlen(parse_table[i].string) + 1;
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

static reset_afm_variables (void)
{
  int i;
  descent = 0.0; ascent = 0.0;
  bbllx = 0.0; bblly = 0.0;
  bburx = 0.0; bbury = 0.0;
  capheight = 0.0; italicangle = 0.0;
  isfixed = 0;
}

static void open_afm_file (const char *tex_name)
{
  static char *afm_name, *full_afm_name;
  afm_name = NEW (strlen (tex_name) + 5, char);
  strcpy (afm_name, tex_name);
  strcat (afm_name, ".afm");
  full_afm_name = kpse_find_file (afm_name, kpse_dvips_header_format,
				  1);
  if (full_afm_name == NULL) {
    fprintf (stderr, "%s: ", afm_name);
    ERROR ("type1_font_descriptor:  Unable to find AFM file");
  }
  fprintf (stderr, "(%s", afm_name);
  release (afm_name);
  if ((type1_afm_file = fopen (full_afm_name, "r")) == NULL) {
    fprintf (stderr, "type1_font_descriptor:  %s\n", afm_name);
    ERROR ("type1_font_descriptor:  Unable to open AFM file");
  }
}
static void close_afm_file (void)
{
  fprintf (stderr, ")");
  fclose (type1_afm_file);
}


static void scan_afm_file (void)
{
  int token, num_char_metrics;
  while ((token = get_afm_token()) >= 0) {
    switch (token) {
    case FONTNAME:
      if (sscanf (position, " %s ", fontname) != 1)
	ERROR ("afm: Error reading Fontname");
      break;
    case CAPHEIGHT:
      if (sscanf (position, " %lf", &capheight) != 1)
	ERROR ("afm: Error reading Capheight");
      break;
    case ASCENT:
      if (sscanf (position, " %lf", &ascent) != 1)
	ERROR ("afm: Error reading ascent");
      break;
    case DESCENT:
      if (sscanf (position, " %lf", &descent) != 1)
	ERROR ("afm: Error reading descent");
      break;
    case ISFIXED:
      if (strstr (position, "false"))
	isfixed = 0;
      else if (strstr (position, "true"))
	isfixed = 1;
      else 
	ERROR ("Can't read value for IsFixedPitch");
      break;
    case FONTBBOX:
      if (sscanf (position, " %lf %lf %lf %lf ", &bbllx, &bblly, &bburx, &bbury) != 4)
	ERROR ("afm: Error reading FontBBox");
      break;
    case ITALICANGLE:
      if (sscanf (position, " %lf", &italicangle) != 1)
	ERROR ("afm: Error reading descent");
      break;
    case XHEIGHT:
      if (sscanf (position, " %lf", &xheight) != 1)
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
}

#define FIXED_WIDTH 1
#define ITALIC 64
#define SYMBOLIC 4   /* Fonts that don't have Adobe encodings (e.g.,
			cmr, should be set to be symbolic */
#define NOCLUE 20

pdf_obj *type1_font_descriptor (const char *tex_name)
{
  pdf_obj *font_descriptor, *font_descriptor_ref, *tmp1;
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
  pdf_add_dict (font_descriptor,
		pdf_new_name ("FontFile"),
		type1_fontfile (tex_name));
  font_descriptor_ref = pdf_ref_obj (font_descriptor);
  pdf_release_obj (font_descriptor);
  return font_descriptor_ref;
}

pdf_obj *type1_font_resource (const char *tex_name, int tfm_font_id, const char *resource_name)
{
  int i;
  int firstchar, lastchar;
  pdf_obj *font_resource, *font_resource_ref, *tmp1, *tmp2;
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
  open_afm_file (tex_name);
  reset_afm_variables ();
  scan_afm_file();
  close_afm_file ();
  if (!is_a_base_font (fontname)) {
    pdf_add_dict (font_resource, 
		  pdf_new_name ("FontDescriptor"),
		  type1_font_descriptor (tex_name));
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
    /*  pdf_new_number (ROUND(char_widths[i],0.01))); */
  }
  pdf_add_dict (font_resource,
		pdf_new_name ("Widths"),
		tmp1);
  font_resource_ref = pdf_ref_obj (font_resource);
  pdf_release_obj (font_resource);
  return font_resource_ref;
}
