/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/vf.c,v 1.7 1998/12/09 21:51:52 mwicks Exp $

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

#include <stdio.h>
#include <ctype.h>
#include "pdflimits.h"
#include "numbers.h"
#include "kpathsea/tex-file.h"
#include "mem.h"
#include "error.h"
#include "tfm.h"
#include "pdfdev.h"
#include "dvi.h"

#include "dvicodes.h"

#define VF_ID 202
#define FIX_WORD_BASE 1048576.0
#define TEXPT2PT (72.0/72.27)
#define FW2PT (TEXPT2PT/((double)(FIX_WORD_BASE)))

static verbose = 1, debug = 0;

struct font_def {
  signed long font_id /* id used internally in vf file */;
  unsigned long checksum, size, design_size;
  char *directory, *name;
  int tfm_id;  /* id returned by TFM module */
  int dev_id;  /* id returned by DEV module */
};


typedef struct font_def dev_font;

struct vf 
{
  char *tex_name;
  double ptsize, mag;
  unsigned long design_size; /* A fixword-pts quantity */
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
  return;
}

static int read_header(FILE *vf_file, int thisfont) 
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
    
    vf_fonts[thisfont].design_size =
      get_unsigned_quad(vf_file);
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
  return;
}

static void read_a_char_def(FILE *vf_file, int thisfont, unsigned long pkt_len,
			    unsigned long ch)
{
  unsigned char *pkt;
  if (debug)
    fprintf (stderr, "read_a_char_def: len=%ld, ch=%ld\n", pkt_len,
	     ch);
  if (pkt_len > 0) {
    pkt = NEW (pkt_len, unsigned char);
    /* Skip TFM width--we already have it somewhere else */
    get_unsigned_triple (vf_file);
    if (fread (pkt, 1, pkt_len, vf_file) != pkt_len)
      ERROR ("VF file ended prematurely.");
    (vf_fonts[thisfont].ch_pkt)[ch] = pkt;
  }
  (vf_fonts[thisfont].pkt_len)[ch] = pkt_len;
  return;
}

static void read_a_font_def(FILE *vf_file, signed long font_id, int thisfont)
{
  dev_font *dev_font;
  int dir_length, name_length;
  if (debug) {
    fprintf (stderr, "read_a_font_def: font_id = %ld\n", font_id);
  }
  if (vf_fonts[thisfont].num_dev_fonts >=
      vf_fonts[thisfont].max_dev_fonts) {
    vf_fonts[thisfont].max_dev_fonts += VF_ALLOC_SIZE;
    vf_fonts[thisfont].dev_fonts = RENEW
      (vf_fonts[thisfont].dev_fonts,
       vf_fonts[thisfont].max_dev_fonts,
       struct font_def);
  }
  dev_font = vf_fonts[thisfont].dev_fonts+
    vf_fonts[thisfont].num_dev_fonts;
  dev_font -> font_id = font_id;
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
  vf_fonts[thisfont].num_dev_fonts += 1;
  dev_font->tfm_id = tfm_open (dev_font -> name);
  dev_font->dev_id =
    dev_locate_font (dev_font->name, 
		     ((double)dev_font->size)*FW2PT*vf_fonts[thisfont].ptsize);
  if (debug) {
    fprintf (stderr, "[%s/%s]\n", dev_font -> directory, dev_font -> name);
  }
  return;
}


void process_vf_file (FILE *vf_file, int thisfont)
{
  int eof = 0, code;
  unsigned long font_id;
  while (!eof) {
    code = get_unsigned_byte (vf_file);
    switch (code) {
    case FNT_DEF1:
      font_id = get_unsigned_byte (vf_file);
      read_a_font_def (vf_file, font_id, thisfont);
      break;
    case FNT_DEF2:
      font_id = get_unsigned_pair (vf_file);
      read_a_font_def (vf_file, font_id, thisfont);
      break;
    case FNT_DEF3:
      font_id = get_unsigned_triple(vf_file);
      read_a_font_def (vf_file, font_id, thisfont);
      break;
    case FNT_DEF4:
      font_id = get_signed_quad(vf_file);
      read_a_font_def (vf_file, font_id, thisfont);
      break;
    default:
      if (code < 242) {
	long ch;
	/* For a short packet, code is the pkt_len */
	ch = get_unsigned_byte (vf_file);
	read_a_char_def (vf_file, thisfont, code, ch);
	break;
      }
      if (code == 243) {
	unsigned long pkt_len, ch;
	pkt_len = get_unsigned_quad(vf_file);
	ch = get_unsigned_quad (vf_file);
	if (ch < 256) 
	  read_a_char_def (vf_file, thisfont, pkt_len, ch);
	else
	  ERROR ("Long character in VF file.  I can't handle long characters!\n");
	break;
      }
      if (code == POST) {
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

/* Unfortunately, the following code isn't smart enough
   to load the vf only once for multiple point sizes. 
   You will get a separate copy of each VF in memory for
   each point size.  Since VFs are pretty small, I guess
   this is tolerable for now.  In any case, 
   the PDF file will never repeat a physical font name */
/* Note: This code needs to be reentrant since it could recurse */
/* Global variables such as num_vf_fonts require careful attention */
int vf_font_locate (char *tex_name, double ptsize)
{
  int thisfont = -1;
  char *full_vf_file_name;
  FILE *vf_file;
  if (verbose)
    fprintf (stderr, "VF: (%s@%g pt)\n", tex_name, ROUND(ptsize,0.01));
  full_vf_file_name = kpse_find_file (tex_name, 
				      kpse_vf_format,
				      1);
  if (full_vf_file_name &&
      (vf_file = fopen (full_vf_file_name, "rb")) != NULL) {
    if (verbose) {
      fprintf (stderr, "VF path: %s\n", full_vf_file_name);
    }
    if (num_vf_fonts >= max_vf_fonts) {
      resize_vf_fonts (max_vf_fonts + VF_ALLOC_SIZE);
    }
    thisfont = num_vf_fonts++;
    vf_fonts[thisfont].ptsize = ptsize;
    read_header(vf_file, thisfont);
    vf_fonts[thisfont].mag =
      ptsize/(vf_fonts[thisfont].design_size*FW2PT);
    clear_vf_characters();
    process_vf_file (vf_file, thisfont);
    fclose (vf_file);
  }
  return thisfont;
}

#define next_byte() (*((*start)++))
static UNSIGNED_BYTE unsigned_byte (unsigned char **start, unsigned char *end)
{
  UNSIGNED_BYTE byte = 0;
  if (*start < end)
    byte = next_byte();
  else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return byte;
}

static SIGNED_BYTE signed_byte (unsigned char **start, unsigned char *end)
{
  int byte = 0;
  if (*start < end) {
    byte = next_byte();
    if (byte >= 0x80) 
      byte -= 0x100;
  }
  else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return (SIGNED_BYTE) byte;
}

static UNSIGNED_PAIR unsigned_pair (unsigned char **start, unsigned char *end)
{
  int i;
  UNSIGNED_BYTE byte;
  UNSIGNED_PAIR pair = 0;
  if (end-*start > 1) {
    for (i=0; i<2; i++) {
      byte = next_byte();
      pair = pair*0x100u + byte;
    }
  }
  else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return pair;
}

static SIGNED_PAIR signed_pair (unsigned char **start, unsigned char *end)
{
  int i;
  long pair = 0;
  if (end - *start > 1) {
    for (i=0; i<2; i++) {
      pair = pair*0x100 + next_byte();
    }
    if (pair >= 0x8000) {
      pair -= 0x10000l;
    }
  } else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return (SIGNED_PAIR) pair;
}

static UNSIGNED_TRIPLE unsigned_triple(unsigned char **start, unsigned
				    char *end)
{
  int i;
  long triple = 0;
  if (end-*start > 2) {
    for (i=0; i<3; i++) {
      triple = triple*0x100u + next_byte();
    }
  } else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return (UNSIGNED_TRIPLE) triple;
}

static SIGNED_TRIPLE signed_triple(unsigned char **start, unsigned char *end)
{
  int i;
  long triple = 0;
  if (end-*start > 2) {
    for (i=0; i<3; i++) {
      triple = triple*0x100 + next_byte();
    }
    if (triple >= 0x800000l) 
       triple -= 0x1000000l;
  } else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return (SIGNED_TRIPLE) triple;
}

static SIGNED_QUAD signed_quad(unsigned char **start, unsigned char *end)
{
  int byte, i;
  long quad = 0;
  /* Check sign on first byte before reading others */
  if (end-*start > 3) {
    byte = next_byte();
    quad = byte;
    if (quad >= 0x80) 
      quad = byte - 0x100;
    for (i=0; i<3; i++) {
      quad = quad*0x100 + next_byte();
    }
  } else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return (SIGNED_QUAD) quad;
}

static UNSIGNED_QUAD unsigned_quad(unsigned char **start, unsigned char *end)
{
  int i;
  unsigned long quad = 0;
  if (end-*start > 3) {
    for (i=0; i<4; i++) {
      quad = quad*0x100u + next_byte();
    }
  } else
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return (UNSIGNED_QUAD) quad;
}

static void vf_set (SIGNED_QUAD ch)
{
  /* Defer to the dvi_set() defined in dvi.c */
  dvi_set (ch);
  return;
}

static void vf_set1(unsigned char **start, unsigned char *end) 
{
  vf_set (unsigned_byte(start, end));
  return;
}

static void vf_putrule(unsigned char **start, unsigned char *end, double fw2dvi)
{
  SIGNED_QUAD width, height;
  height = signed_quad (start, end);
  width = signed_quad (start, end);
  if (width > 0 && height > 0) {
    dvi_rule (width*fw2dvi, height*fw2dvi);
  }
  return;
}

static void vf_setrule(unsigned char **start, unsigned char *end, double fw2dvi)
{
  SIGNED_QUAD width, height;
  height = signed_quad (start, end);
  width = signed_quad (start, end);
  if (width > 0 && height > 0) {
    dvi_rule (width*fw2dvi, height*fw2dvi);
  }
  dvi_right (width*fw2dvi);
  return;
}

static void vf_put1(unsigned char **start, unsigned char *end)
{
  dvi_put (unsigned_byte(start, end));
  return;
}

static void vf_push(void)
{
  dvi_push();
  return;
}

static void vf_pop(void)
{
  dvi_pop();
  return;
}

static void vf_right (SIGNED_QUAD x, double fw2dvi)
{
  dvi_right ((SIGNED_QUAD) (x*fw2dvi));
  return;
}


static void vf_right1(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_right (signed_byte (start, end), fw2dvi);
  return;
}

static void vf_right2(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_right (signed_pair (start, end), fw2dvi);
  return;
}

static void vf_right3(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_right (signed_triple (start, end), fw2dvi);
  return;
}

static void vf_right4(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_right (signed_quad (start, end), fw2dvi);
  return;
}

static void vf_w0(void)
{
  dvi_w0();
  return;
}

static void vf_w (SIGNED_QUAD w, double fw2dvi)
{
  dvi_w ((SIGNED_QUAD) (w*fw2dvi));
  return;
}

static void vf_w1(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_w (signed_byte(start, end), fw2dvi);
  return;
}

static void vf_w2(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_w (signed_pair(start, end), fw2dvi);
  return;
}

static void vf_w3(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_w (signed_triple(start, end), fw2dvi);
  return;
}

static void vf_w4(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_w (signed_quad(start, end), fw2dvi);
  return;
}

static void vf_x0(void)
{
  dvi_x0();
  return;
}

static void vf_x (SIGNED_QUAD x, double fw2dvi)
{
  dvi_x ((SIGNED_QUAD) (x*fw2dvi));
  return;
}

static void vf_x1(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_x (signed_byte(start, end), fw2dvi);
  return;
}

static void vf_x2(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_x (signed_pair(start, end), fw2dvi);
  return;
}

static void vf_x3(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_x (signed_triple(start, end), fw2dvi);
  return;
}

static void vf_x4(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_x (signed_quad(start, end), fw2dvi);
  return;
}

static void vf_down (SIGNED_QUAD y, double fw2dvi)
{
  dvi_down ((SIGNED_QUAD) (y*fw2dvi));
  return;
}

static void vf_down1(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_down (signed_byte(start, end), fw2dvi);
  return;
}

static void vf_down2(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_down (signed_pair(start, end), fw2dvi);
  return;
}

static void vf_down3(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_down (signed_triple(start, end), fw2dvi);
  return;
}

static void vf_down4(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_down (signed_quad(start, end), fw2dvi);
  return;
}

static void vf_y0(void)
{
  dvi_y0();
  return;
}

static void vf_y (SIGNED_QUAD y, double fw2dvi)
{
  dvi_y ((SIGNED_QUAD) (y*fw2dvi));
  return;
}


static void vf_y1(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_y (signed_byte(start, end), fw2dvi);
  return;
}

static void vf_y2(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_y (signed_pair(start, end), fw2dvi);
  return;
}

static void vf_y3(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_y (signed_triple(start, end), fw2dvi);
  return;
}

static void vf_y4(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_y (signed_quad(start, end), fw2dvi);
  return;
}

static void vf_z0(void)
{
  dvi_z0();
  return;
}

static void vf_z (SIGNED_QUAD z, double fw2dvi)
{
  dvi_z ((SIGNED_QUAD) (z*fw2dvi));
  return;
}

static void vf_z1(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_z (signed_byte(start, end), fw2dvi);
  return;
}

static void vf_z2(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_z (signed_pair(start, end), fw2dvi);
  return;
}

static void vf_z3(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_z (signed_triple(start, end), fw2dvi);
  return;
}

static void vf_z4(unsigned char **start, unsigned char *end, double fw2dvi)
{
  vf_z (signed_quad(start, end), fw2dvi);
  return;
}

static void vf_fnt (SIGNED_QUAD font_id, unsigned long vf_font)
{
  int i;
  for (i=0; i<vf_fonts[vf_font].num_dev_fonts; i++) {
    if (font_id == ((vf_fonts[vf_font].dev_fonts)[i]).font_id) {
      break;
    }
  }
  if (i < vf_fonts[vf_font].num_dev_fonts) { /* Font was found */
    dev_select_font ((vf_fonts[vf_font].dev_fonts[i]).dev_id);
  } else {
    fprintf (stderr, "Font_id: %ld not found in VF\n", font_id);
  }
  return;
}


static void vf_fnt1(unsigned char **start, unsigned char *end,
		    unsigned long vf_font)
{
  vf_fnt (unsigned_byte(start, end), vf_font);
  return;
}

static void vf_fnt2(unsigned char **start, unsigned char *end,
		    unsigned long vf_font)
{
  vf_fnt (unsigned_pair(start, end), vf_font);
  return;
}

static void vf_fnt3(unsigned char **start, unsigned char *end,
		    unsigned long vf_font)
{
  vf_fnt (unsigned_triple(start, end), vf_font);
  return;
}

static void vf_fnt4(unsigned char **start, unsigned char *end,
		    unsigned long vf_font)
{
  vf_fnt (signed_quad(start, end), vf_font);
  return;
}

static void vf_xxx (SIGNED_QUAD len, unsigned char **start, unsigned
		    char *end)
{
  /* For now, skip specials */
  if (end-*start >= len)
    *start += len;
  else 
    ERROR ("Premature end of DVI byte stream in VF font\n");
  return;
}

static void vf_xxx1(unsigned char **start, unsigned char *end)
{
  vf_xxx (unsigned_byte(start, end), start, end);
  return;
}

static void vf_xxx2(unsigned char **start, unsigned char *end)
{
  vf_xxx (unsigned_pair(start, end), start, end);
  return;
}

static void vf_xxx3(unsigned char **start, unsigned char *end)
{
  vf_xxx (unsigned_triple(start, end), start, end);
  return;
}

static void vf_xxx4(unsigned char **start, unsigned char *end)
{
  vf_xxx (unsigned_quad(start, end), start, end);
  return;
}

static double vf_set_scale (int vf_font)
{
  double result;
  result = vf_fonts[vf_font].ptsize/(double)FIX_WORD_BASE;
  result /= dvi_unit_size();
 /* Note: DVI magnification is include in dvi_unit_size() */
  return result;
}

void vf_set_char(int ch, int vf_font)
{
  unsigned char opcode;
  unsigned char *start, *end;
  int default_font = -1;
  double fw2dvi;
  if (vf_font < num_vf_fonts) {
    /* Initialize to the first font or -1 if undefined */
    fw2dvi = vf_set_scale(vf_font);
    if (vf_fonts[vf_font].num_dev_fonts > 0)
      default_font = ((vf_fonts[vf_font].dev_fonts)[0]).dev_id;
    dvi_vf_init (default_font);
    /*    dev_select_font (default_font); */
    start = (vf_fonts[vf_font].ch_pkt)[ch];
    end = start + (vf_fonts[vf_font].pkt_len)[ch];
    while (start < end) {
      opcode = *(start++);
      if (debug) {
	fprintf (stderr, "Opcode: %d", opcode);
	if (isprint (opcode)) fprintf (stderr, " (%c)\n", opcode);
	else  fprintf (stderr, "\n");
      }
      switch (opcode)
	{
	case SET1:
	  vf_set1(&start, end);
	break;
	case SET2:
	case SET3:
	case SET4:
	  ERROR ("dvi_do_page: Multibyte byte character in DVI file.  I can't handle this!");
	  break;
	case SET_RULE:
	  vf_setrule(&start, end, fw2dvi);
	  break;
	case PUT1:
	  vf_put1(&start, end);
	  break;
	case PUT2:
	case PUT3:
	case PUT4:
	  ERROR ("dvi_do_page: Multibyte byte character in DVI file.  I can't handle this!");
	  break;
	case PUT_RULE:
	  vf_putrule(&start, end, fw2dvi);
	  break;
	case NOP:
	  break;
	case PUSH:
	  vf_push();
	  break;
	case POP:
	  vf_pop();
	  break;
	case RIGHT1:
	  vf_right1(&start, end, fw2dvi);
	  break;
	case RIGHT2:
	  vf_right2(&start, end, fw2dvi);
	  break;
	case RIGHT3:
	  vf_right3(&start, end, fw2dvi);
	  break;
	case RIGHT4:
	  vf_right4(&start, end, fw2dvi);
	  break;
	case W0:
	  vf_w0();
	  break;
	case W1:
	  vf_w1(&start, end, fw2dvi);
	  break;
	case W2:
	  vf_w2(&start, end, fw2dvi);
	  break;
	case W3:
	  vf_w3(&start, end, fw2dvi);
	  break;
	case W4:
	  vf_w4(&start, end, fw2dvi);
	  break;
	case X0:
	  vf_x0();
	  break;
	case X1:
	  vf_x1(&start, end, fw2dvi);
	  break;
	case X2:
	  vf_x2(&start, end, fw2dvi);
	  break;
	case X3:
	  vf_x3(&start, end, fw2dvi);
	  break;
	case X4:
	  vf_x4(&start, end, fw2dvi);
	  break;
	case DOWN1:
	  vf_down1(&start, end, fw2dvi);
	  break;
	case DOWN2:
	  vf_down2(&start, end, fw2dvi);
	  break;
	case DOWN3:
	  vf_down3(&start, end, fw2dvi);
	  break;
	case DOWN4:
	  vf_down4(&start, end, fw2dvi);
	  break;
	case Y0:
	  vf_y0();
	  break;
	case Y1:
	  vf_y1(&start, end, fw2dvi);
	  break;
	case Y2:
	  vf_y2(&start, end, fw2dvi);
	  break;
	case Y3:
	  vf_y3(&start, end, fw2dvi);
	  break;
	case Y4:
	  vf_y4(&start, end, fw2dvi);
	  break;
	case Z0:
	  vf_z0();
	  break;
	case Z1:
	  vf_z1(&start, end, fw2dvi);
	  break;
	case Z2:
	  vf_z2(&start, end, fw2dvi);
	  break;
	case Z3:
	  vf_z3(&start, end, fw2dvi);
	  break;
	case Z4:
	  vf_z4(&start, end, fw2dvi);
	  break;
	case FNT1:
	  vf_fnt1(&start, end, vf_font);
	  break;
	case FNT2:
	  vf_fnt2(&start, end, vf_font);
	  break;
	case FNT3:
	  vf_fnt3(&start, end, vf_font);
	  break;
	case FNT4:
	  vf_fnt4(&start, end, vf_font);
	  break;
	case XXX1:
	  vf_xxx1(&start, end);
	  break;
	case XXX2:
	  vf_xxx2(&start, end);
	  break;
	case XXX3:
	  vf_xxx3(&start, end);
	  break;
	case XXX4:
	  vf_xxx4(&start, end);
	  break;
	default:
	  if (opcode <= SET_CHAR_127) {
	    vf_set (opcode);
	  } else if (opcode >= FNT_NUM_0 && opcode <= FNT_NUM_63) {
	    vf_fnt (opcode - FNT_NUM_0, vf_font);
	  } else {
	    fprintf (stderr, "Unexpected opcode: %d\n", opcode);
	    ERROR ("Unexpected opcode in vf file\n");
	  }
	}
    }
    dvi_vf_finish();
  } else {
    fprintf (stderr, "vf_set_char: font: %d", vf_font);
    ERROR ("Font not loaded\n");
  }
  return;
}


void vf_close_all_fonts(void)
{
  unsigned long i;
  int j;
  dev_font *one_font;
#ifdef MEM_DEBUG
MEM_START
#endif
  for (i=0; i<num_vf_fonts; i++) {
    /* Release the packet for each character */
    for (j=0; j<256; j++) {
      if ((vf_fonts[i].ch_pkt)[j] != NULL)
	RELEASE ((vf_fonts[i].ch_pkt)[j]);
    }
    /* Release each font record */
    for (j=0; j<vf_fonts[i].num_dev_fonts; j++) {
      one_font = &(vf_fonts[i].dev_fonts)[j];
      RELEASE (one_font -> directory);
      RELEASE (one_font -> name);
    }
    if (vf_fonts[i].dev_fonts != NULL)
      RELEASE (vf_fonts[i].dev_fonts);
  }
  if (vf_fonts != NULL)
    RELEASE (vf_fonts);
#ifdef MEM_DEBUG
MEM_END
#endif
  return;
}
