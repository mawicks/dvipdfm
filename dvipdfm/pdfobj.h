/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfobj.h,v 1.20 1999/02/09 03:24:08 mwicks Exp $

    This is dvipdf, a DVI to PDF translator.
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

	
#ifndef PDFOBJ_H
#define PDFOBJ_H
#include <stdio.h>

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
  unsigned long label;  /* Only used for indirect objects
                               all other "label" to zero */
  unsigned generation;  /* Only used if "label" is used */
  unsigned refcount;  /* Number of links to this object */
};
typedef struct pdf_obj pdf_obj;

struct pdf_boolean
{
  char value;
};
typedef struct pdf_boolean pdf_boolean;


struct pdf_number
{
  double value;
};
typedef struct pdf_number pdf_number;

struct pdf_string
{
  unsigned char *string;
  unsigned length;
};
typedef struct pdf_string pdf_string;


struct pdf_name
{
  char *name;
};
typedef struct pdf_name pdf_name;


struct pdf_array
{
  unsigned long max, size;
  pdf_obj **values;
};
typedef struct pdf_array pdf_array;

struct pdf_dict
{
  pdf_obj *key;
  pdf_obj *value;
  struct pdf_dict *next;
};
typedef struct pdf_dict pdf_dict;

struct pdf_stream
{
  struct pdf_obj *dict;
  char *stream;  /* Actual stream contents stored in "stream" */
  unsigned long stream_length;
  unsigned long max_length;
  unsigned char _flags;
};
typedef struct pdf_stream pdf_stream;

struct pdf_indirect
{
  unsigned label, generation;
  int dirty;  /* Dirty objects came from an input file and were not
		 generated by this program.  They have a label in a
		 different numbering sequence.  These are translated
		 when the object is written out */
  FILE *dirty_file;
};
typedef struct pdf_indirect pdf_indirect;

/* External interface to pdf routines */

extern void pdf_obj_set_verbose(void);

struct pdf_obj *pdf_new_obj (pdf_obj_type type);

extern void pdf_out_init (const char *filename);

extern void pdf_out_flush (void);

extern pdf_obj *pdf_new_obj(pdf_obj_type type);

extern pdf_obj *pdf_link_obj(pdf_obj *object);

extern pdf_obj *pdf_new_null (void);

extern pdf_obj *pdf_new_boolean (char value);
extern void pdf_set_boolean (pdf_obj *object, char value);

extern void pdf_set_info (pdf_obj *object);
extern void pdf_set_root (pdf_obj *object);
     

extern pdf_obj *pdf_new_number (double value);
extern void pdf_set_number (pdf_obj *object, double value);
extern double pdf_number_value (pdf_obj *number);

extern char *pdf_name_value (pdf_obj *object);

extern pdf_obj *pdf_new_string (const void *string, unsigned length);
extern void pdf_set_string (pdf_obj *object, unsigned char *string, unsigned length);
extern void *pdf_string_value (pdf_obj *object);

#define pdf_obj_string_value(s) ((void *)(((pdf_string *)((s)->data)) -> string))
#define pdf_obj_string_length(s) (((pdf_string *)((s)->data)) -> length)

extern pdf_obj *pdf_new_name (const char *name);  /* Name does not include the / */
extern int pdf_check_name (const char *name);  /* Tell whether name is a
					   valid PDF name */

extern pdf_obj *pdf_new_array (void);
extern void pdf_add_array (pdf_obj *array, pdf_obj *object); /* Array is ended
							 by a node with NULL
							 this pointer */
extern pdf_obj *pdf_get_array (pdf_obj *array, unsigned long index);
extern pdf_obj *pdf_new_dict (void);
extern void pdf_add_dict (pdf_obj *dict, pdf_obj *key, pdf_obj *value);  /* Array is ended
								     by a node with NULL
								     this pointer */
extern void pdf_merge_dict (pdf_obj *dict1, pdf_obj *dict2);
extern pdf_obj *pdf_lookup_dict (const pdf_obj *dict, const char *name);
extern char *pdf_get_dict (const pdf_obj *dict, int index);

extern pdf_obj *pdf_new_stream (int flags);
#define STREAM_COMPRESS 1
extern void pdf_obj_set_compression(int level);
extern void pdf_add_stream (pdf_obj *stream, char *stream_data, unsigned
		     length);

extern pdf_obj *pdf_stream_dict(pdf_obj *stream);

extern void pdf_release_obj (pdf_obj *object);
extern pdf_obj *pdf_deref_obj (pdf_obj *object);
extern pdf_obj *pdf_ref_obj (pdf_obj *object);
extern pdf_obj *pdf_new_ref (unsigned long label, int generation);
extern pdf_obj *pdf_ref_file_obj (unsigned long obj_no);
extern void pdf_write_obj (FILE *file, const pdf_obj *object);
extern pdf_obj *pdf_open (char *filename);
extern void pdf_close (void);

extern int pdfobj_escape_str (char *buffer, int size, unsigned char *s, int len);
extern int check_for_pdf (FILE *file);

#endif  /* PDFOBJ_H */

