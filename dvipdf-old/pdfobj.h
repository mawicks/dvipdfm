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
#define PDF_INDIRECT	8u   

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
};


/* External interface to pdf routines */

struct pdf_obj *pdf_new_obj (pdf_obj_type type);

void pdf_out_init (const char *filename);

void pdf_out_flush (void);

pdf_obj *pdf_new_obj(pdf_obj_type type);

pdf_obj *pdf_new_boolean (char value);
void pdf_set_boolean (pdf_obj *object, char value);

pdf_obj *pdf_new_number (double value);
void pdf_set_number (pdf_obj *object, double value);

pdf_obj *pdf_new_string (const char *string, unsigned length);
void pdf_set_string (pdf_obj *object, char *string, unsigned length);

pdf_obj *pdf_new_name (const char *name);  /* Name does not include the / */
pdf_obj *pdf_new_array (void);
void pdf_add_array (pdf_obj *array, pdf_obj *object); /* Array is ended
							 by a node with NULL
							 this pointer */

pdf_obj *pdf_new_dict (void);
void pdf_add_dict (pdf_obj *dict, pdf_obj *key, pdf_obj *value);  /* Array is ended
								     by a node with NULL
								     this pointer */
pdf_obj *pdf_new_stream (void);
void pdf_add_stream (pdf_obj *stream, char *stream_data, unsigned
		     length);
#define pdf_stream_dict(s) (((struct pdf_stream *)((s)->data))->dict) 
void pdf_release_obj (pdf_obj *object);
pdf_obj *pdf_ref_obj (pdf_obj *object);

int pdfobj_escape_c (char *buffer, char ch);

#endif  /* PDFOBJ_H */



