/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/mpost.c,v 1.6 1999/08/26 04:51:02 mwicks Exp $

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

#include <stdlib.h>
#include <ctype.h>
#include "system.h"
#include "mfileio.h"
#include "pdfobj.h"
#include "pdfspecial.h"
#include "pdfparse.h"
#include "mpost.h"
#include "string.h"
#include "pdfparse.h"
#include "mem.h"
#include "pdflimits.h"
#include "error.h"
#include "pdfdev.h"
#include "pdfdoc.h"

int check_for_mp (FILE *image_file) 
{
  rewind (image_file);
  /* For now, this is an exact test that must be passed, character for
     character */
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strncmp (work_buffer, "%!PS", 4))
    return 0;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strlen(work_buffer)>=strlen("%%BoundingBox") &&
      strncmp (work_buffer, "%%BoundingBox", strlen("%%BoundingBox")))
    return 0;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strlen(work_buffer) >= strlen("%%Creator: MetaPost") &&
      strncmp (work_buffer, "%%Creator: MetaPost", strlen("%%Creator: MetaPost")))
    return 0;
  fprintf (stderr, "\nMetapost file\n");
  return 1;
}
static struct mp_fonts 
{
  char *tex_name;
  int font_id;
  double pt_size;
} mp_fonts[MAX_FONTS];
int n_mp_fonts = 0;

int mp_locate_font (char *tex_name, double pt_size)
{
  int result, i;
  for (i=0; i<n_mp_fonts; i++) {
    if (!strcmp (tex_name, mp_fonts[i].tex_name) &&
	mp_fonts[i].pt_size == pt_size)
      break;
  }
  if (n_mp_fonts >= MAX_FONTS) {
    ERROR ("Too many fonts in mp_locate_font()");
  }
  if (i == n_mp_fonts) {
    n_mp_fonts += 1;
    mp_fonts[i].tex_name = NEW (strlen(tex_name), char);
    strcpy (mp_fonts[i].tex_name, tex_name);
    mp_fonts[i].pt_size = pt_size;
    /* The following line is a bit of a kludge.  MetaPost inclusion
       was an afterthought */
    mp_fonts[i].font_id = dev_locate_font (tex_name,
					   pt_size/dev_dvi2pts());
    result = mp_fonts[i].font_id;
  } else 
    result = -1;
  fprintf (stderr, "mp_locate_font() =%d\n", result);
  return result;
}

int mp_is_font_name (char *tex_name)
{
  int i;
  for (i=0; i<n_mp_fonts; i++) {
    if (!strcmp (tex_name, mp_fonts[i].tex_name))
      break;
  }
  if (i < n_mp_fonts)
    return 1;
  else
    return 0;
}

int mp_fontid (char *tex_name, double pt_size)
{
  int i;
  for (i=0; i<n_mp_fonts; i++) {
    fprintf (stderr, "stored=%g,ptsize=%g\n", mp_fonts[i].pt_size, pt_size);
    if (!strcmp (tex_name, mp_fonts[i].tex_name) &&
	(mp_fonts[i].pt_size == pt_size))
      break;
  }
  if (i < n_mp_fonts) {
    fprintf (stderr, "returning=%d\n", i);
    return i;
  }
  else
    fprintf (stderr, "returning=%d\n", -1);
    return -1;
}

static double bbllx, bblly, bburx, bbury;
int mp_parse_headers (FILE *image_file)
{
  char *start, *end, *token, *name;
  unsigned long save_position;
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  /* Presumably already checked file, so press on */
  mfgets (work_buffer, WORK_BUFFER_SIZE, image_file);
  if (strncmp (work_buffer, "%%BoundingBox:", strlen("%%BoundingBox")))
    return 0;
  start = work_buffer + strlen("%%BoundingBox:");
  end = start+strlen(start);
  skip_white (&start, end);
  /* Expect 4 numbers or fail */
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bbllx = atof (token);
    RELEASE(token);
  } else
    return 0;
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bblly = atof (token);
    RELEASE(token);
  } else
    return 0;
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bburx = atof (token);
    RELEASE(token);
  } else
    return 0;
  if ((token = parse_number (&start, end))) {
    skip_white (&start, end);
    bbury = atof (token);
    RELEASE(token);
  } else
    return 0;
  /* Got four numbers */
  fprintf (stderr, "bbllx=%g,bblly=%g,bburx=%g,bbury=%g\n",
	   bbllx,bblly,bburx,bbury);
  /* Read all headers--act on *Font records */
  save_position = tell_position(image_file);
  while (!feof(image_file) && mfgets (work_buffer, WORK_BUFFER_SIZE,
				     image_file)) {
    if (*work_buffer != '%') {
      seek_absolute (image_file, save_position);
      break;
    }
    save_position = tell_position (image_file);
    if (*(work_buffer+1) == '*' &&
	!strncmp (work_buffer+2, "Font:", strlen("Font:"))) {
      double ps_ptsize;
      start = work_buffer+strlen("%*Font:");
      end = start+strlen(start);
      skip_white (&start, end);
      if ((name = parse_ident (&start, end))) {
	skip_white (&start, end);
      } else
	return 0;
      if ((token = parse_number (&start, end))) {
	ps_ptsize = atof (token);
	RELEASE (token);
      } else
	return 0;
      fprintf (stderr, "name=%s,size=%g\n", name, ps_ptsize);
      mp_locate_font (name, ps_ptsize);
    }
  }
  return 1;
}

#define PS_STACK_SIZE 1024
static pdf_obj *stack[PS_STACK_SIZE];
static top_stack;

double x_state, y_state;

#define PUSH(o) { \
  if (top_stack<PS_STACK_SIZE) { \
    stack[top_stack++] = o; \
  } else { \
    fprintf (stderr, "PS stack overflow including MetaPost file"); \
    return; \
  } \
} \

#define POP_STACK() (top_stack>0?stack[--top_stack]:NULL)

void dump_stack()
{
  int i;
  fprintf (stderr, "\ndump_stack\n");
  for (i=0; i<top_stack; i++) {
    pdf_write_obj (stderr, stack[i]);
    fprintf (stderr, "\n");
  }
}
#define ADD          	1
#define CLIP         	2
#define CLOSEPATH    	3
#define CONCAT       	4
#define CURVETO   	5
#define DIV		6
#define DTRANSFORM	7
#define EXCH		8
#define FILL		9
#define FSHOW		10
#define GSAVE		11
#define GRESTORE	12
#define IDTRANSFORM	13
#define LINETO		14
#define MOVETO		15
#define MUL		16
#define NEWPATH		17
#define POP		18
#define RLINETO		19
#define SCALE		20
#define SETCMYKCOLOR	21
#define SETDASH		22
#define SETGRAY		23
#define SETLINECAP	24
#define SETLINEJOIN	25
#define SETLINEWIDTH	26
#define SETMITERLIMIT	27
#define SETRGBCOLOR	28
#define SHOW		29
#define SHOWPAGE	30
#define STROKE		31
#define SUB		32
#define TRANSLATE	33
#define TRUNCATE	34
#define FONTNAME	99

struct operators 
{
  char *t;
  int v;
} operators[] = {
  {"add", ADD},
  {"clip", CLIP},
  {"closepath", CLOSEPATH},
  {"concat", CONCAT},
  {"curveto", CURVETO},
  {"div", DIV},
  {"dtransform", DTRANSFORM},
  {"exch", EXCH},
  {"fill", FILL},
  {"fshow", FSHOW},
  {"gsave", GSAVE},
  {"grestore", GRESTORE},
  {"idtransform", IDTRANSFORM},
  {"lineto", LINETO},
  {"moveto", MOVETO},
  {"mul", MUL},
  {"newpath", NEWPATH},
  {"pop", POP},
  {"rlineto", RLINETO},
  {"scale", SCALE},
  {"setcmykcolor", SETCMYKCOLOR},
  {"setdash", SETDASH},
  {"setgray", SETGRAY},
  {"setlinecap", SETLINECAP},
  {"setlinejoin", SETLINEJOIN},
  {"setlinewidth", SETLINEWIDTH},
  {"setmiterlimit", SETMITERLIMIT},
  {"setrgbcolor", SETRGBCOLOR},
  {"show", SHOW},
  {"showpage", SHOWPAGE},
  {"stroke", STROKE},  
  {"sub", SUB},  
  {"translate", TRANSLATE},
  {"truncate", TRUNCATE}
};


static int lookup_operator(char *token)
{
  int i, operator;
  operator = -1;
  for (i=0; i<sizeof(operators)/sizeof(operators[0]); i++) {
    if (!strcmp (token, operators[i].t)) {
      operator = operators[i].v;
      break;
    }
  }
  if (i == sizeof(operators)/sizeof(operators[0]) &&
      mp_is_font_name (token)) {
    operator = FONTNAME;
  }
  return operator;
}

static void do_operator(char *token)
{
  int operator;
  pdf_obj *tmp1=NULL, *tmp2=NULL, *tmp3=NULL, *tmp4 = NULL;
  pdf_obj *tmp5=NULL, *tmp6=NULL;
  int len;
  fprintf (stderr, "\n\n\ndo_operator(%s)", token);
  dump_stack();
  operator = lookup_operator (token);
  fprintf (stderr, "op=%d\n\n\n", operator);
  switch (operator) {
  case ADD:
    tmp1 = POP_STACK();
    tmp2 = POP_STACK();
    if (tmp1 && tmp2)
      pdf_set_number (tmp1,
		      pdf_number_value(tmp1)+pdf_number_value(tmp2));
    if (tmp2)
      pdf_release_obj (tmp2);
    if (tmp1)
      PUSH(tmp1);
    break;
  case CLIP:
    pdf_doc_add_to_page (" W", 2);
    break;
  case CLOSEPATH:
    pdf_doc_add_to_page (" h", 2);
    break;
  case CONCAT:
    tmp1 = POP_STACK();
    if (tmp1 && tmp1 -> type == PDF_ARRAY) {
      int i, len = 0;
      for (i=0; i<6; i++) {
	if (!(tmp2 = pdf_get_array(tmp1, i)))
	  break;
	len += sprintf (work_buffer+len, " %g",
			pdf_number_value(tmp2));
      }
      len += sprintf (work_buffer+len, " cm");
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nMissing array before \"concat\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case CURVETO:
    if ((tmp6 = POP_STACK()) && tmp6->type == PDF_NUMBER &&
	(tmp5 = POP_STACK()) && tmp5->type == PDF_NUMBER &&
	(tmp4 = POP_STACK()) && tmp4->type == PDF_NUMBER &&
	(tmp3 = POP_STACK()) && tmp3->type == PDF_NUMBER &&
	(tmp2 = POP_STACK()) && tmp2->type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g %g %g %g %g %g c", 
		     pdf_number_value(tmp1), pdf_number_value(tmp2),
		     pdf_number_value(tmp3), pdf_number_value(tmp4),
		     pdf_number_value(tmp5), pdf_number_value(tmp6));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nMissing number(s) before \"curveto\"\n");
    }
    if (tmp1) pdf_release_obj (tmp1);
    if (tmp2) pdf_release_obj (tmp2);
    if (tmp3) pdf_release_obj (tmp3);
    if (tmp4) pdf_release_obj (tmp4);
    if (tmp5) pdf_release_obj (tmp5);
    if (tmp6) pdf_release_obj (tmp6);
    break;
 case DIV:
    tmp2 = POP_STACK();
    tmp1 = POP_STACK();
    if (tmp1 && tmp2)
      pdf_set_number (tmp1,
		      pdf_number_value(tmp1)/pdf_number_value(tmp2));
    if (tmp1)
      PUSH(tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case DTRANSFORM:
    if ((tmp2 = POP_STACK()) && tmp2 -> type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1 -> type == PDF_NUMBER) {
      pdf_set_number (tmp1, pdf_number_value(tmp1)*100.0);
      pdf_set_number (tmp2, pdf_number_value(tmp2)*100.0);
      PUSH(tmp1);
      PUSH(tmp2);
    } else {
      if (tmp1)
	pdf_release_obj (tmp1);
      if (tmp2)
	pdf_release_obj (tmp2);
      fprintf (stderr, "\nExpecting two numbers before \"dtransform\"");
    }
    break;
  case EXCH:
    if ((tmp1 = POP_STACK()) &&
	(tmp2 = POP_STACK())) {
      PUSH (tmp1);
      PUSH (tmp2);
    } else {
      if (tmp1)
	pdf_release_obj (tmp1);
    }
    break;
  case FILL:
    pdf_doc_add_to_page (" f", 2);
    break;
  case FSHOW: 
    {
      int fontid;
      if ((tmp3 = POP_STACK()) && (tmp3->type == PDF_NUMBER) &&
	  (tmp2 = POP_STACK()) && (tmp2->type == PDF_STRING) &&
	  (tmp1 = POP_STACK()) && (tmp1->type == PDF_STRING)) {
	if ((fontid = mp_fontid (pdf_string_value(tmp2),
				 pdf_number_value(tmp3))) < 0) {
	  fprintf (stderr, "\n\"fshow\": Missing font in MetaPost file? %s@%g\n", 
		   (char *) pdf_string_value(tmp2), pdf_number_value(tmp3));
	}
	dev_set_string (x_state/dev_dvi2pts(), y_state/dev_dvi2pts(),
			pdf_string_value(tmp1),
			pdf_string_length(tmp1), 0, fontid);
	graphics_mode();
      }
      if (tmp1)
	pdf_release_obj (tmp1);
      if (tmp2)
	pdf_release_obj (tmp2);
      if (tmp3)
	pdf_release_obj (tmp3);
    }
    break;
  case GSAVE:
    pdf_doc_add_to_page (" q", 2);
    break;
  case GRESTORE:
    pdf_doc_add_to_page (" Q", 2);
    break;
  case IDTRANSFORM:
    dump_stack();
    if ((tmp2 = POP_STACK()) && tmp2 -> type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1 -> type == PDF_NUMBER) {
      pdf_set_number (tmp1, pdf_number_value(tmp1)/100.0);
      pdf_set_number (tmp2, pdf_number_value(tmp2)/100.0);
      PUSH(tmp1);
      PUSH(tmp2);
    } else {
      if (tmp1)
	pdf_release_obj (tmp1);
      if (tmp2)
	pdf_release_obj (tmp2);
      fprintf (stderr, "\nExpecting two numbers before \"idtransform\"");
    }
    break;
  case LINETO: 
    {
      int len;
      if ((tmp2 = POP_STACK()) && tmp2-> type == PDF_NUMBER &&
	  (tmp1 = POP_STACK()) && tmp1-> type == PDF_NUMBER) {
	len = sprintf (work_buffer, " %g %g l",
		 pdf_number_value (tmp1),pdf_number_value (tmp2));
	pdf_doc_add_to_page (work_buffer, len);
	x_state = pdf_number_value (tmp1);
	y_state = pdf_number_value (tmp2);
      }
      if (tmp1)
	pdf_release_obj (tmp1);
      if (tmp2)
	pdf_release_obj (tmp2);
    }
    break;
  case MOVETO:
    fprintf (stderr, "moveto\n");
    dump_stack();
    
    if ((tmp2 = POP_STACK()) && tmp2-> type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1-> type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g %g m",
		     pdf_number_value (tmp1),pdf_number_value (tmp2));
      pdf_doc_add_to_page (work_buffer, len);
      x_state = pdf_number_value (tmp1);
      y_state = pdf_number_value (tmp2);
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case MUL:
    tmp2 = POP_STACK();
    tmp1 = POP_STACK();
    if (tmp1 && tmp2)
      pdf_set_number (tmp1,
		      pdf_number_value(tmp1)*pdf_number_value(tmp2));
    if (tmp1)
      PUSH(tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case NEWPATH:
    pdf_doc_add_to_page (" n", 2);
    break;
  case POP:
    tmp1 = POP_STACK();
    if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case RLINETO: 
    if ((tmp2 = POP_STACK()) && tmp2->type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g %g l",
		     x_state+pdf_number_value(tmp1),
		     y_state+pdf_number_value(tmp2));
      pdf_doc_add_to_page (work_buffer, len);
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case SCALE: 
    if ((tmp2 = POP_STACK()) &&  tmp2->type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g 0 0 %g 0 0 cm", 
		     pdf_number_value (tmp1),
		     pdf_number_value (tmp2));
      pdf_doc_add_to_page (work_buffer, len);
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case SETCMYKCOLOR:
    if ((tmp4 = POP_STACK()) && tmp4->type == PDF_NUMBER &&
	(tmp3 = POP_STACK()) && tmp3->type == PDF_NUMBER &&
	(tmp2 = POP_STACK()) && tmp2->type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g %g %g %g k",
		     pdf_number_value (tmp1), pdf_number_value (tmp2),
		     pdf_number_value (tmp3), pdf_number_value (tmp4));
      pdf_doc_add_to_page (work_buffer, len);
      len = sprintf (work_buffer, " %g %g %g %g K",
		     pdf_number_value (tmp1), pdf_number_value (tmp2),
		     pdf_number_value (tmp3), pdf_number_value (tmp4));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nExpecting four numbers before \"setcmykcolor\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    if (tmp3)
      pdf_release_obj (tmp3);
    if (tmp4)
      pdf_release_obj (tmp4);
    break;
  case SETDASH:
    fprintf (stderr, "setdash\n"); dump_stack();
    if ((tmp2 = POP_STACK()) && tmp2->type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1->type == PDF_ARRAY) {
      int i;
      pdf_doc_add_to_page (" [", 2);
      for (i=0;; i++) {
	if ((tmp3 = pdf_get_array (tmp1, i)) &&
	    tmp3 -> type == PDF_NUMBER) {
	  len = sprintf (work_buffer, " %g", pdf_number_value(tmp3));
	  pdf_doc_add_to_page (work_buffer, len);
	} else 
	  break;
      }
      pdf_doc_add_to_page (" ]", 2);
      if (tmp2 -> type == PDF_NUMBER) {
	len = sprintf (work_buffer, " %g d", pdf_number_value(tmp2));
	pdf_doc_add_to_page (work_buffer, len);
      }
    } else {
      fprintf (stderr, "\nExpecting array and number before \"setdash\"");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case SETGRAY:
    if ((tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g g",
		     pdf_number_value (tmp1));
      pdf_doc_add_to_page (work_buffer, len);
      len = sprintf (work_buffer, " %g G",
		     pdf_number_value (tmp1));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nExpecting a number before \"setgray\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case SETLINECAP:
    if ((tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g J", pdf_number_value (tmp1));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nExpecting a number before \"setlinecap\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case SETLINEJOIN:
    if ((tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g j", pdf_number_value (tmp1));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nExpecting a number before \"setlinejoin\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case SETLINEWIDTH:
    if ((tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g w", pdf_number_value (tmp1));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nExpecting a number before \"setlinewidth\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case SETMITERLIMIT:
    if ((tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g M", pdf_number_value (tmp1));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nExpecting a number before \"setmiterlimit\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case SETRGBCOLOR:
    if ((tmp3 = POP_STACK()) && tmp3->type == PDF_NUMBER &&
	(tmp2 = POP_STACK()) && tmp2->type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " %g %g %g rg",
		     pdf_number_value (tmp1), pdf_number_value (tmp2),
		     pdf_number_value (tmp3));
      pdf_doc_add_to_page (work_buffer, len);
      len = sprintf (work_buffer, " %g %g %g RG",
		     pdf_number_value (tmp1), pdf_number_value (tmp2),
		     pdf_number_value (tmp3));
      pdf_doc_add_to_page (work_buffer, len);
    } else {
      fprintf (stderr, "\nExpecting three numbers before \"setrgbcolor\"\n");
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    if (tmp3)
      pdf_release_obj (tmp3);
    break;
  case SHOWPAGE:
    /* Let's ignore this for now */
    break;
  case STROKE:
    pdf_doc_add_to_page (" S", 2);
    break;
  case SUB:
    tmp2 = POP_STACK();
    tmp1 = POP_STACK();
    if (tmp1 && tmp2)
      pdf_set_number (tmp1,
		      pdf_number_value(tmp1)-pdf_number_value(tmp2));
    if (tmp1)
      PUSH(tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case TRANSLATE:
    if ((tmp2 = POP_STACK()) &&  tmp2->type == PDF_NUMBER &&
	(tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      len = sprintf (work_buffer, " 1 0 0 1 %g %g cm", 
		     pdf_number_value (tmp1),
		     pdf_number_value (tmp2));
      pdf_doc_add_to_page (work_buffer, len);
    }
    if (tmp1)
      pdf_release_obj (tmp1);
    if (tmp2)
      pdf_release_obj (tmp2);
    break;
  case TRUNCATE:
    if ((tmp1 = POP_STACK()) && tmp1->type == PDF_NUMBER) {
      double val=pdf_number_value(tmp1);
      pdf_set_number (tmp1, val>0? floor(val): ceil(val));
      PUSH (tmp1);
    } else if (tmp1)
      pdf_release_obj (tmp1);
    break;
  case FONTNAME:
    PUSH (pdf_new_string (token, strlen(token)));
    break;
  default: 
    fprintf (stderr, "\nUnknown operator: %s\n", token);
    ERROR ("Unknown operator in Metapost file\n");
  }
}

static char line_buffer[1024];
void parse_contents (FILE *image_file)
{
  top_stack = 0;
  fprintf (stderr, "parse_contents()\n");
  x_state = 0.0;
  y_state = 0.0;
  while (!feof(image_file) && mfgets (line_buffer, sizeof(line_buffer),
				      image_file)) {
    char *start, *end, *token;
    pdf_obj *obj;
    start = line_buffer;
    end = start+strlen(line_buffer);
    skip_white (&start, end);
    while (start < end) {
      if (isdigit (*start) || *start == '-') {
	token = parse_number (&start, end);
	PUSH (pdf_new_number(atof(token)));
	RELEASE (token);
      } else if (*start == '[' && /* This code assumes that arrays are contained on one line */
		 (obj = parse_pdf_array (&start, end))) {
	PUSH (obj);
      } else if (*start == '(' &&
		 (obj = parse_pdf_string (&start, end))) {
	PUSH (obj);
      } else {
	token = parse_ident (&start, end);
	do_operator (token);
	RELEASE (token);
      }
      skip_white (&start, end);
    }
    /*    dump_stack(); */
  }
}

/* mp inclusion is a bit of a hack.  The routine
 * starts a form at the lower left corner of
 * the page and then calls begin_form_xobj telling
 * it to record the image drawn there and bundle it
 * up in an xojbect.  This allows us to use the coordinates
 * in the MP file directly.  This appears to be the
 * easiest way to be able to use the dev_set_string()
 * command (with its scaled and extended fonts) without
 * getting all confused about the coordinate system.
 * After the xobject is created, the whole thing can
 * be scaled any way the user wants */
 
pdf_obj *mp_include (FILE *image_file,  struct xform_info *p,
		     char *res_name, double x_user, double y_user)
{
   pdf_obj *xobj = NULL;
   rewind (image_file);
   if (mp_parse_headers (image_file)) {
      /* Looks like an MP file.  Setup xobj "capture" */
     xobj = begin_form_xobj (bbllx,bblly, bbllx, bblly,
			     bburx, bbury, res_name);
      if (!xobj)
	return NULL;
      /* Flesh out the contents */
      parse_contents (image_file);
      fprintf (stderr, "Returned from parse_contents\n");

      /* Finish off the form */
      end_form_xobj();
      /* Add it to the resource list */
      pdf_doc_add_to_page_xobjects (res_name, pdf_ref_obj(xobj));
      /* And release it */
      pdf_release_obj (xobj);
      sprintf (work_buffer, " q 1 0 0 1 %.2f %.2f cm /%s Do Q",
	       x_user, y_user, res_name);
      pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
   }
   return NULL;
}







