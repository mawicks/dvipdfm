
/* This is tailored for PDF */

#include <string.h>
#include <stdio.h>
#include "pdfobj.h"
#include "mem.h"
#include "error.h"

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


static unsigned long do_pfb_segment (FILE *file, int expected_type, pdf_obj *stream)
{
  int i, ch;
  static char buffer[256];
  unsigned long length;
  if ((ch = fgetc (file)) < 0 || ch != 128){
    fprintf (stderr, "Got %d, expecting 128\n", ch);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  if ((ch = fgetc (file)) < 0 || ch != expected_type ) {
    fprintf (stderr, "Got %d, expecting %d\n", ch, expected_type);
    ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
  }
  length = get_low_endian_quad (file);
  fprintf (stderr, "reading %d byte segment \n", length);
  /* Speed this loop up later... NEEDS WORK */ 
  for (i=0; i<length; i++) {
    if ((ch = fgetc (file)) < 0) {
      fprintf (stderr, "Found only %d bytes\n", i);
      ERROR ("type1_do_pfb_segment:  Are you sure this is a pfb?");
    }
    if (ch == '\r')
      ch = '\n';  /* May not be portable to non-Unix systems */
    buffer[0] = ch;
    pdf_add_stream (stream, buffer, 1);  /* Terribly slow */
  }
  return length;
}

pdf_obj *type1_fontfile (char *tex_name)
{
  FILE *type1_binary_file;
  pdf_obj *stream, *stream_dict, *stream_label, *tmp1, *tmp2;
  unsigned long length1, length2, length3;
  char *pfb_name;
  int ch;
  pfb_name = NEW (strlen (tex_name) + 5, char);
  strcpy (pfb_name, tex_name);
  strcat (pfb_name, ".pfb");
  if ((type1_binary_file = fopen (pfb_name, "rb")) == NULL) {
    fprintf (stderr, "type1_fontfile:  %s\n", pfb_name);
    ERROR ("type1_fontfile:  Unable to open binary font file");
  }
  stream = pdf_new_stream();
  /* Following line doesn't hide PDF stream structure very well */
  length1 = do_pfb_segment (type1_binary_file, 1, stream);
  length2 = do_pfb_segment (type1_binary_file, 2, stream);
  length3 = do_pfb_segment (type1_binary_file, 1, stream);
  if ((ch = fgetc (type1_binary_file)) != 128 ||
      (ch = fgetc (type1_binary_file)) != 3)
    ERROR ("type1_fontfile:  Are you sure this is a pfb?");
  /* Got entire file! */
  fclose (type1_binary_file);
  stream_dict = pdf_stream_dict (stream);
  pdf_add_dict (stream_dict, tmp1 = pdf_new_name("Length1"),
		tmp2 = pdf_new_number (length1));
  pdf_release_obj (tmp1);  pdf_release_obj (tmp2);
  pdf_add_dict (stream_dict, tmp1 = pdf_new_name("Length2"),
		tmp2 = pdf_new_number (length2));
  pdf_release_obj (tmp1);  pdf_release_obj (tmp2);
  pdf_add_dict (stream_dict, tmp1 = pdf_new_name("Length3"),
		tmp2 = pdf_new_number (length3));
  pdf_release_obj (tmp1);  pdf_release_obj (tmp2);
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
static int firstchar, lastchar;
static int isfixed;
static char *fontname[256];  /* Make as long as buffer */
static double char_widths[256];

static reset_afm_variables (void)
{
  int i;
  descent = 0.0; ascent = 0.0;
  bbllx = 0.0; bblly = 0.0;
  bburx = 0.0; bbury = 0.0;
  capheight = 0.0; italicangle = 0.0;
  firstchar = 255; lastchar = 0;
  isfixed = 0;
  for (i=0; i<256; i++) {
    char_widths[i] = 0.0;
  }
}

static scan_char_metrics (int num_expected)
{
  int ch, afm_tok, i;
  double width;
  char *p, *tok;
  fprintf (stderr, "num_expected: %d\n", num_expected);
  for (i=0; i<num_expected; i++) {
    if ((p = fgets (buffer, sizeof(buffer), type1_afm_file)) == NULL){
      ERROR ("scan_char_metrics:  Couldn't read all the expected metrics");
    }
    ch = -1; width = 0;
    while (tok = strtok (p, ";")) {
      p = NULL;
      if (strstr (tok, "C ")){
	sscanf (tok, " C %d", &ch);
      }
      else if (strstr (tok, "WX ")){
	sscanf (tok, " WX %lf", &width);
      }
    }
    if (ch >= 0) {
      char_widths[ch] = width;
      if (ch > lastchar) lastchar = ch;
      if (ch < firstchar) firstchar = ch;
    }
  }
  for (i=firstchar; i<=lastchar; i++) {
    fprintf (stderr, "wd[%d] = %f\n", i, char_widths[i]);
  }
}

static void open_afm_file (char *tex_name)
{
  static char *afm_name;
  afm_name = NEW (strlen (tex_name) + 5, char);
  strcpy (afm_name, tex_name);
  strcat (afm_name, ".afm");
  if ((type1_afm_file = fopen (afm_name, "r")) == NULL) {
    fprintf (stderr, "type1_font_descriptor:  %s\n", afm_name);
    ERROR ("type1_font_descriptor:  Unable to open AFM file");
  }
}

pdf_obj *type1_font_descriptor (char *tex_name)
{
   pdf_obj *font_descriptor, *tmp1, *tmp2;
  int token, num_char_metrics;
  open_afm_file (tex_name);
  reset_afm_variables ();
  while ((token = get_afm_token()) >= 0) {
    switch (token) {
    case FONTNAME:
      if (sscanf (position, " %s ", fontname) != 1)
	ERROR ("afm: Error reading Fontname");
      fprintf (stderr, "Fontname: (%s)\n", fontname);
      break;
    case CAPHEIGHT:
      if (sscanf (position, " %lf", &capheight) != 1)
	ERROR ("afm: Error reading Capheight");
      fprintf (stderr, "Capheight: %f\n", capheight);
      break;
    case ASCENT:
      if (sscanf (position, " %lf", &ascent) != 1)
	ERROR ("afm: Error reading ascent");
      fprintf (stderr, "Ascent: %lf\n", ascent);
      break;
    case DESCENT:
      if (sscanf (position, " %lf", &descent) != 1)
	ERROR ("afm: Error reading descent");
      fprintf (stderr, "Descent: %lf\n", descent);
      break;
    case ISFIXED:
      if (strstr (position, "false"))
	isfixed = 0;
      else if (strstr (position, "true"))
	isfixed = 1;
      else 
	ERROR ("Can't read value for IsFixedPitch");
      fprintf(stderr, "isfixed %d\n", isfixed);
      break;
    case FONTBBOX:
      if (sscanf (position, " %lf %lf %lf %lf ", &bbllx, &bblly, &bburx, &bbury) != 4)
	ERROR ("afm: Error reading FontBBox");
      fprintf (stderr, "FontBBox: %lf %lf %lf %lf \n", bbllx, bblly,
	       bburx, bbury);
      break;
    case ITALICANGLE:
      if (sscanf (position, " %lf", &italicangle) != 1)
	ERROR ("afm: Error reading descent");
      fprintf (stderr, "ItalicAngle: %lf\n", italicangle);
      break;
    case XHEIGHT:
      if (sscanf (position, " %lf", &xheight) != 1)
	ERROR ("afm: Error reading XHeight");
      fprintf (stderr, "XHeight: %lf\n", xheight);
      break;
    case SCHARMETRICS:
      if (sscanf (position, " %ld", &num_char_metrics) != 1)
	ERROR ("afm:  Error reading number of char metrics");
      scan_char_metrics(num_char_metrics);
      break;
    case COMMENT:
    case SFONTMETRICS:
    case EFONTMETRICS:
    case ECHARMETRICS:
      break;
    }
  }
}









