/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/vf.c,v 1.1 1998/12/08 14:09:56 mwicks Exp $

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
#include "kpathsea/tex-file.h"
#include "mem.h"
#include "error.h"
#include "tfm.h"
#include "pdfdev.h"

#include "dvicodes.h"

#define VF_ID 202
#define FIX_WORD_BASE 1048576
#define TEXPT2PT (72.0/72.27)
#define FW2PT (TEXPT2PT/((double)(FIX_WORD_BASE)))

static verbose = 1;


struct font_def {
  signed long vf_id /* id used internally in vf file */;
  unsigned long checksum, size, design_size;
  char *directory, *name;
  int tfm_id;  /* id returned by TFM module */
  int dev_id;  /* id returned by DEV module */
};

typedef struct font_def dev_font;

struct vf 
{
  char *tex_name;
  unsigned long design_size;
  int num_dev_fonts, max_dev_fonts;
  dev_font *dev_fonts;
  unsigned char *ch_pkt[256];
  unsigned long pkt_len[256];
};

struct vf *vf_fonts = NULL;
int num_vf_fonts = 0, max_vf_fonts = 0;

static void clear_vf_characters(void)
{
  int i;
  for (i=0; i<256; i++){
    (vf_fonts[num_vf_fonts].pkt_len)[i] = 0;
    (vf_fonts[num_vf_fonts].ch_pkt)[i] = NULL;
  }
}


static int read_header(FILE *vf_file) 
{
  int i, result = 1, ch;

  /* Check for usual signature */
  if ((ch = get_unsigned_byte (vf_file)) == PRE &&
      (ch = get_unsigned_byte (vf_file)) == VF_ID) {

    /* If here, assume it's a legitimate vf file */
    ch = get_unsigned_byte (vf_file);

    /* skip comment */
    for (i=0; i<ch; i++)
      get_unsigned_byte (vf_file);

    /* Skip checksum */
    get_unsigned_quad(vf_file);
    
    vf_fonts[num_vf_fonts].design_size =
      get_unsigned_quad(vf_file);
    if (verbose) 
      fprintf (stderr, "Design size: %g (TeX) pts\n",
	       (double)vf_fonts[num_vf_fonts].design_size/(double)
	       FIX_WORD_BASE);
  } else { /* Try to fail gracefully and return an error to caller */
    fprintf (stderr, "VF file may be corrupt\n");
    result = 0;
  }
  return result;
}


static void resize_vf_fonts(int size)
{
  int i;
  if (size > max_vf_fonts) {
    vf_fonts = RENEW (vf_fonts, size, struct vf);
    for (i=max_vf_fonts; i<size; i++) {
      vf_fonts[i].num_dev_fonts = 0;
      vf_fonts[i].max_dev_fonts = 0;
      vf_fonts[i].dev_fonts = NULL;
    }
    max_vf_fonts = size;
  }
}

static void read_a_char_def(FILE *vf_file, unsigned long pkt_len,
			    unsigned long ch)
{
  unsigned char *pkt;
  if (verbose)
    fprintf (stderr, "read_a_char_def: len=%ld, ch=%ld\n", pkt_len,
	     ch);
  if (pkt_len > 0) {
    pkt = NEW (pkt_len, unsigned char);
    /* Skip TFM width--we already have it somewhere else */
    get_unsigned_triple (vf_file);
    if (fread (pkt, 1, pkt_len, vf_file) != pkt_len)
      ERROR ("VF file ended prematurely.");
    (vf_fonts[num_vf_fonts].ch_pkt)[ch] = pkt;
    {
      int i;
      fputc ('[', stderr);
      for (i=0; i<pkt_len; i++) {
	fputc (pkt[i], stderr);
      }
      fprintf (stderr, "]\n");
    }
  }
  (vf_fonts[num_vf_fonts].pkt_len)[ch] = pkt_len;
  return;
}

static void read_a_font_def(FILE *vf_file, signed long vf_id)
{
  dev_font *dev_font;
  int dir_length, name_length;
  if (verbose) {
    fprintf (stderr, "read_a_font_def: vf_id = %ld\n", vf_id);
  }
  if (vf_fonts[num_vf_fonts].num_dev_fonts >=
      vf_fonts[num_vf_fonts].max_dev_fonts) {
    vf_fonts[num_vf_fonts].max_dev_fonts += VF_ALLOC_SIZE;
    vf_fonts[num_vf_fonts].dev_fonts = RENEW
      (vf_fonts[num_vf_fonts].dev_fonts,
       vf_fonts[num_vf_fonts].max_dev_fonts,
       struct font_def);
  }
  dev_font = vf_fonts[num_vf_fonts].dev_fonts+
    vf_fonts[num_vf_fonts].num_dev_fonts;
  dev_font -> vf_id = vf_id;
  dev_font -> checksum = get_unsigned_quad (vf_file);
  dev_font -> size = get_unsigned_quad (vf_file);
  dev_font -> design_size = get_unsigned_quad (vf_file);
  dir_length = get_unsigned_byte (vf_file);
  name_length = get_unsigned_byte (vf_file);
  dev_font -> directory = NEW (dir_length+1, char);
  dev_font -> name = NEW (name_length+1, char);
  fread (dev_font -> directory, 1, dir_length, vf_file);
  fread (dev_font -> name, 1, name_length, vf_file);
  (dev_font -> directory)[dir_length] = 0;
  (dev_font -> name)[name_length] = 0;
  vf_fonts[num_vf_fonts].num_dev_fonts += 1;
  dev_font->tfm_id = tfm_open (dev_font -> name);
  dev_font->dev_id = dev_locate_font (dev_font->name, 
				      dev_font->tfm_id,
				      ((double)dev_font->design_size)*FW2PT);
  if (verbose) {
    fprintf (stderr, "[%s/%s]\n", dev_font -> directory, dev_font -> name);
  }
  
  return;
}


void process_vf_file (FILE *vf_file)
{
  int eof = 0, code;
  unsigned long font_id;
  while (!eof) {
    code = get_unsigned_byte (vf_file);
    switch (code) {
    case FNT_DEF1:
      fprintf (stderr, "FNT_DEF1\n");
      font_id = get_unsigned_byte (vf_file);
      read_a_font_def (vf_file, font_id);
      break;
    case FNT_DEF2:
      fprintf (stderr, "FNT_DEF2\n");
      font_id = get_unsigned_pair (vf_file);
      read_a_font_def (vf_file, font_id);
      break;
    case FNT_DEF3:
      fprintf (stderr, "FNT_DEF3\n");
      font_id = get_unsigned_triple(vf_file);
      read_a_font_def (vf_file, font_id);
      break;
    case FNT_DEF4:
      fprintf (stderr, "FNT_DEF4\n");
      font_id = get_signed_quad(vf_file);
      read_a_font_def (vf_file, font_id);
      break;
    default:
      if (code < 242) {
	long ch;
	/* For a short packet, code is the pkt_len */
	fprintf (stderr, "CHAR_PKT[%d]\n", code);
	ch = get_unsigned_byte (vf_file);
	read_a_char_def (vf_file, code, ch);
	break;
      }
      if (code == 243) {
	unsigned long pkt_len, ch;
	fprintf (stderr, "LONG CHAR_PKT\n");
	pkt_len = get_unsigned_quad(vf_file);
	ch = get_unsigned_quad (vf_file);
	ERROR ("I can't handle long character packets\n");
	break;
      }
      if (code == POST) {
	fprintf (stderr, "POST\n");
	eof = 1;
	break;
      }
      fprintf (stderr, "Quitting on code=%d\n", code);
      eof = 1;
      break;
    }
  }
  return;
}


int vf_font_locate (char *tex_name)
{
  int result = -1;
  char *full_vf_file_name;
  FILE *vf_file;
  full_vf_file_name = kpse_find_file (tex_name, 
				      kpse_vf_format,
				      1);
  if (full_vf_file_name &&
      (vf_file = fopen (full_vf_file_name, "rb")) != NULL) {
    if (verbose) {
      fprintf (stderr, "Found: %s\n", full_vf_file_name);
    }
    if (num_vf_fonts >= max_vf_fonts) {
      resize_vf_fonts (max_vf_fonts + VF_ALLOC_SIZE);
    }
    if (read_header(vf_file)) {
      if (verbose)
	fprintf (stderr, "Header looks good...\n");
    }
    clear_vf_characters();
    process_vf_file(vf_file);
    result = ++num_vf_fonts;
  }
  return result;
}
