/*
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

	
#ifndef PDFOBJ_H
#define PDFOBJ_H
#include <stdio.h>

#define PDF_MAX_IND_OBJECTS 1024

/* Here is the complete list of PDF object types */

#define	PDF_BOOLEAN	1u
#define	PDF_NUMBER	2u
#define PDF_STRING	3u
#define PDF_NAME	4u
#define PDF_ARRAY	5u
#define PDF_DICT	6u
#define PDF_STREAM	7u
#define PDF_NULL        8u
#define PDF_INDIRECT	9u   

typedef unsigned char pdf_obj_type;

/* Any of these types can be represented as follows */

struct pdf_obj 
{
  pdf_obj_type type;
  void *data;
  unsigned label;  /* Only used for indirect objects
                               all other "label" to zero */
  unsigned generation;  /* Only used if "label" is used */
  unsigned refcount;  /* Number of links to this object */
};


typedef struct pdf_obj pdf_obj;

struct pdf_boolean
{
  char value;
};


struct pdf_number
{
  double value;
};


struct pdf_string
{
  char *string;
  unsigned length;
};


struct pdf_name
{
  char *name;
};


struct pdf_array
{
  pdf_obj *this;
  struct pdf_array *next;
};


struct pdf_dict
{
  pdf_obj *key;
  pdf_obj *value;
  struct pdf_dict *next;
};


struct pdf_stream
{
  struct pdf_obj *dict;
  pdf_obj *length;  /* Pointer to an object containing the length of the stream */
  FILE *tmpfile;  /* Streams are stored in a temp file */
};

struct pdf_indirect
{
  unsigned label, generation;
  int dirty;  /* Dirty objects came from an input file and were not
		 generated by this program.  They have a label in a
		 different numbering sequence.  These are translated
		 when the object is written out */
  FILE *dirty_file;
};


/* External interface to pdf routines */

struct pdf_obj *pdf_new_obj (pdf_obj_type type);

void pdf_out_init (const char *filename);

void pdf_out_flush (void);

pdf_obj *pdf_new_obj(pdf_obj_type type);

pdf_obj *pdf_link_obj(pdf_obj *object);

pdf_obj *pdf_new_null (void);

pdf_obj *pdf_new_boolean (char value);
void pdf_set_boolean (pdf_obj *object, char value);

pdf_obj *pdf_new_number (double value);
void pdf_set_number (pdf_obj *object, double value);
double pdf_number_value (pdf_obj *number);

pdf_obj *pdf_new_string (const char *string, unsigned length);
void pdf_set_string (pdf_obj *object, char *string, unsigned length);
char *pdf_string_value (pdf_obj *object);

#define pdf_obj_string_value(s) (((struct pdf_string *)((s)->data)) -> string)
#define pdf_obj_string_length(s) (((struct pdf_string *)((s)->data)) -> length)

pdf_obj *pdf_new_name (const char *name);  /* Name does not include the / */
int pdf_match_name (const pdf_obj *name_obj, const char *name);  /* Name does not include the / */
int pdf_check_name (const char *name);  /* Tell whether name is a
					   valid PDF name */

pdf_obj *pdf_new_array (void);
void pdf_add_array (pdf_obj *array, pdf_obj *object); /* Array is ended
							 by a node with NULL
							 this pointer */
pdf_obj *pdf_get_array (pdf_obj *array, int index);
pdf_obj *pdf_new_dict (void);
void pdf_add_dict (pdf_obj *dict, pdf_obj *key, pdf_obj *value);  /* Array is ended
								     by a node with NULL
								     this pointer */
void pdf_merge_dict (pdf_obj *dict1, pdf_obj *dict2);
pdf_obj *pdf_lookup_dict (const pdf_obj *dict, const char *name);
char *pdf_get_dict (const pdf_obj *dict, int index);

pdf_obj *pdf_new_stream (void);
void pdf_add_stream (pdf_obj *stream, char *stream_data, unsigned
		     length);

pdf_obj *pdf_stream_dict(pdf_obj *stream);

void pdf_release_obj (pdf_obj *object);
pdf_obj *pdf_deref_obj (pdf_obj *object);
pdf_obj *pdf_ref_obj (pdf_obj *object);
pdf_obj *pdf_new_ref (int label, int generation);
pdf_obj *pdf_ref_file_obj (int obj_no);

void pdf_write_obj (FILE *file, const pdf_obj *object);
pdf_obj *pdf_open (char *filename);

int pdfobj_escape_c (char *buffer, char ch);

#endif  /* PDFOBJ_H */



