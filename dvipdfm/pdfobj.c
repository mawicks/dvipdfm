/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfobj.c,v 1.5 1998/11/30 20:38:25 mwicks Exp $

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

	
#include <ctype.h>
#include <string.h>

#include "system.h" 
#include "pdfobj.h"
#include "mem.h"
#include "error.h"
#include "io.h"
#include "pdfspecial.h"
#include "pdfparse.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

unsigned next_label = 1;
FILE *pdf_output_file = NULL;
FILE *pdf_input_file = NULL;
unsigned long pdf_output_file_position = 0;
int pdf_output_line_position = 0;
char format_buffer[256];

static struct xref_entry 
{
  unsigned long file_position;
  pdf_obj *pdf_obj;
} output_xref[PDF_MAX_IND_OBJECTS];
static unsigned long startxref;

static unsigned pdf_root_obj = 0, pdf_info_obj = 0;

/* Internal static routines */

static void pdf_flush_obj (FILE *file, const pdf_obj *object);
static void pdf_label_obj (pdf_obj *object);

static void pdf_out_char (FILE *file, char c);
static void pdf_out (FILE *file, char *buffer, int length);

static void release_indirect (pdf_indirect *data);
static void write_indirect (FILE *file, const pdf_indirect *indirect);

static void release_boolean (pdf_obj *data);
static void write_boolean (FILE *file, const pdf_boolean *data);

static void write_null (FILE *file);
static void release_null (void *data);

static void release_number (pdf_number *data);
static void write_number (FILE *file, const pdf_number
			  *number);

static void write_string (FILE *file, const pdf_string *string);
static void release_string (pdf_string *data);

static void write_name (FILE *file, const pdf_name *name);
static void release_name (pdf_name *data);

static void write_array (FILE *file, const pdf_array *array);
static void release_array (pdf_array *data);

static void write_dict (FILE *file, const pdf_dict *dict);
static void release_dict (pdf_dict *data);

static void write_stream (FILE *file, const pdf_stream *stream);
static void release_stream (pdf_stream *stream);

static int debug = 0, verbose = 0;

void pdf_obj_set_debug(void)
{
  debug = 1;
}

void pdf_obj_set_verbose(void)
{
  verbose = 1;
}


void pdf_out_init (const char *filename)
{
  if (!(pdf_output_file = fopen (filename, FOPEN_WBIN_MODE))) {
    if (strlen(filename) < 128) {
      sprintf (format_buffer, "Unable to open %s\n", filename);
    } else
      sprintf (format_buffer, "Unable to open file");
    ERROR (format_buffer);
  }
  pdf_out (pdf_output_file, "%PDF-1.2\n", 9);
}

static void dump_xref(void)
{
  int length, i;
  startxref = pdf_output_file_position;	/* Record where this xref is for
				   trailer */
  pdf_out (pdf_output_file, "xref\n", 5);
  length = sprintf (format_buffer, "%d %d\n", 0, next_label);
  pdf_out (pdf_output_file, format_buffer, length);
  length = sprintf (format_buffer, "%010ld %05ld f \n", 0L, 65535L);
  /* Every space counts.  The space after the 'f' and 'n' is
   *essential*.  The PDF spec says the lines must be 20 characters
   long including the end of line character. */
  pdf_out (pdf_output_file, format_buffer, length);
  for (i=1; i<next_label; i++){
    length = sprintf (format_buffer, "%010ld %05ld n \n",
		      output_xref[i-1].file_position, 0L);
    pdf_out (pdf_output_file, format_buffer, length);
  }
}

static void dump_trailer(void)
{
  int length;
  unsigned long starttrailer;
  starttrailer = pdf_output_file_position;
  pdf_out (pdf_output_file, "trailer\n", 8);
  pdf_out (pdf_output_file, "<<\n", 3);
  length = sprintf (format_buffer, "/Size %d\n",
		    next_label);
  pdf_out (pdf_output_file, format_buffer, length);
  if (pdf_root_obj == 0) 
    ERROR ("dump_trailer:  Invalid root object");
  length = sprintf (format_buffer, "/Root %u %u R\n", pdf_root_obj, 0);
  pdf_out (pdf_output_file, format_buffer, length);
  if (pdf_info_obj != 0) {
    length = sprintf (format_buffer, "/Info %u %u R\n", pdf_info_obj, 0);
    pdf_out (pdf_output_file, format_buffer, length);
  }
  pdf_out (pdf_output_file, ">>\n", 3);
  pdf_out (pdf_output_file, "startxref\n", 10);
  length = sprintf (format_buffer, "%ld\n", startxref);
  pdf_out (pdf_output_file, format_buffer, length);
  pdf_out (pdf_output_file, "%%EOF\n", 6);
}


void pdf_out_flush (void)
{
  if (pdf_output_file) {
    if (verbose) fprintf (stderr, "pdf_obj_out_flush:  dumping xref\n");
    dump_xref();
    if (verbose) fprintf (stderr, "pdf_obj_out_flush:  dumping trailer\n");
    dump_trailer();
    fclose (pdf_output_file);
  }
}

void pdf_set_root (pdf_obj *object)
{
  if (pdf_root_obj != 0) {
    ERROR ("pdf_set_root:  root object already set");
  }
  if (object -> label == 0) {  /* Make sure this object has a label */
    pdf_label_obj (object);
  }
  pdf_root_obj = object -> label;
}

void pdf_set_info (pdf_obj *object)
{
  if (pdf_info_obj != 0) {
    ERROR ("pdf_set_info:  info object already set");
  }
  if (object -> label == 0) {  /* Make sure this object has a label */
    pdf_label_obj (object);
  }
  pdf_info_obj = object -> label;
}

static void pdf_out_char (FILE *file, char c)
{
  fputc (c, file);
  /* Keep tallys for xref table *only* if writing a pdf file */
  if (file == pdf_output_file) {
    pdf_output_file_position += 1;
    pdf_output_line_position += 1;
  }
  if (file == pdf_output_file && c == '\n')
    pdf_output_line_position = 0;
}

static void pdf_out (FILE *file, char *buffer, int length)
{
  fwrite (buffer, 1, length, file);
  /* Keep tallys for xref table *only* if writing a pdf file */
  if (file == pdf_output_file) {
    pdf_output_file_position += length;
    pdf_output_line_position += length;
  }
}

static void pdf_out_white (FILE *file)
{
  if (file == pdf_output_file && pdf_output_line_position >= 80) {
    pdf_out_char (file, '\n');
  } else {
    pdf_out_char (file, ' ');
  }
  return;
}

pdf_obj *pdf_new_obj(pdf_obj_type type)
{
  pdf_obj *result;
  result = NEW (1, pdf_obj);
  result -> type = type;
  result -> data = NULL;
  result -> label = 0;
  result -> generation = 0;
  result -> refcount = 1;
  return result;
}

static void pdf_label_obj (pdf_obj *object)
{
  if (object == NULL)
    return;
  if (next_label > PDF_MAX_IND_OBJECTS) {
    ERROR ("pdf_label_obj:  Indirect object capacity exceeded");
  }
  if (object -> label == 0) {  /* Don't change label on an already labeled
				  object.  Ignore such calls */
    /* Save so we can lookup this object by its number */
    output_xref[next_label-1].pdf_obj = object;
    object -> label = next_label++;
    object -> generation = 0;
  }
}

/* This doesn't really copy the object, but allows 
   it to be used without fear that somebody else will free it */

pdf_obj *pdf_link_obj (pdf_obj *object)
{
  if (object == NULL)
    ERROR ("pdf_link_obj passed null pointer");
  object -> refcount += 1;
  return object;
}


pdf_obj *pdf_ref_obj(pdf_obj *object)
{
  pdf_obj *result;
  pdf_indirect *indirect;
  
  if (object == NULL)
    ERROR ("pdf_ref_obj passed null pointer");
  
  if (object -> type == 0) {
    ERROR ("pdf_ref_obj:  Called with invalid object");
  }
  result = pdf_new_obj (PDF_INDIRECT);
  indirect = NEW (1, pdf_indirect);
  result -> data = indirect;
  if (object -> type == PDF_INDIRECT) { /* If an object is already an indirect reference,
					   reference the original
					   object, not the indirect
					   one */
    indirect -> label = ((pdf_indirect *) (object -> data)) -> label;
    indirect -> generation = ((pdf_indirect *) (object -> data)) -> generation;
    indirect -> dirty = ((pdf_indirect *) (object -> data)) -> dirty;
    indirect -> dirty_file = ((pdf_indirect *) (object -> data)) -> dirty_file;
  } else {
    if (object -> label == 0) {
      pdf_label_obj (object);
    }
    indirect -> label = object -> label;
    indirect -> generation = object -> generation;
    indirect -> dirty = 0;
    indirect -> dirty_file = NULL;
  }
  return result;
}

static void release_indirect (pdf_indirect *data)
{
  free (data);
}

static void write_indirect (FILE *file, const pdf_indirect *indirect)
{
  int length;
  if (indirect -> dirty) {
    if (file == stderr) {
      pdf_out (file, "{d}", 3);
      length = sprintf (format_buffer, "%d %d R", indirect -> label,
			indirect -> generation);
      pdf_out (file, format_buffer, length);
    }
    else {
      pdf_obj *clean;
      if (indirect -> dirty_file != pdf_input_file) {
	fprintf (stderr, "\nwrite_indirect, label=%d, from_file=%p, current_file=%p\n", indirect -> label, indirect->dirty_file, pdf_input_file);
	ERROR ("write_indirect:  input PDF file doesn't match object");
      }
      clean = pdf_ref_file_obj (indirect -> label);
      pdf_write_obj (file, clean);
      pdf_release_obj (clean);
    }
  } else {
    length = sprintf (format_buffer, "%d %d R", indirect -> label,
		      indirect -> generation);
    pdf_out (file, format_buffer, length);
  }
}

pdf_obj *pdf_new_null (void)
{
  pdf_obj *result;
  result = pdf_new_obj (PDF_NULL);
  result -> data = NULL;
  return result;
}

static void release_null (void *data)
{
  return;
}

static void write_null (FILE *file)
{
  pdf_out (file, "null", 4);
}

pdf_obj *pdf_new_boolean (char value)
{
  pdf_obj *result;
  pdf_boolean *data;
  result = pdf_new_obj (PDF_BOOLEAN);
  data = NEW (1, pdf_boolean);
  result -> data = data;
  data -> value = value;
  return result;
}

static void release_boolean (pdf_obj *data)
{
  free (data);
}

static void write_boolean (FILE *file, const pdf_boolean *data)
{
  if (data -> value) {
    pdf_out (file, "true", 4);
  }
  else {
    pdf_out (file, "false", 5);
  }
}

void pdf_set_boolean (pdf_obj *object, char value)
{
   if (object == NULL || object -> type != PDF_BOOLEAN) {
     ERROR ("pdf_set_boolean:  Passed non-boolean object");
   }
   ((pdf_boolean *) (object -> data)) -> value = value;
}

pdf_obj *pdf_new_number (double value)
{
  pdf_obj *result;
  pdf_number *data;
  result = pdf_new_obj (PDF_NUMBER);
  data = NEW (1, pdf_number);
  result -> data = data;
  data -> value = value;
  return result;
}

static void release_number (pdf_number *data)
{
  free (data);
}

static void write_number (FILE *file, const pdf_number *number)
{
  int count;
  count = sprintf (format_buffer, "%g", number -> value);
  pdf_out (file, format_buffer, count);
}


void pdf_set_number (pdf_obj *object, double value)
{
   if (object == NULL || object -> type != PDF_NUMBER) {
     ERROR ("pdf_set_number:  Passed non-number object");
   }
   ((pdf_number *) (object -> data)) -> value = value;
}

double pdf_number_value (pdf_obj *object)
{
  if (object == NULL || object -> type != PDF_NUMBER) {
    ERROR ("pdf_obj_number_value:  Passed non-number object");
  }
  return ((pdf_number *)(object -> data)) -> value;
}

pdf_obj *pdf_new_string (const unsigned char *string, unsigned length)
{
  pdf_obj *result;
  pdf_string *data;
  result = pdf_new_obj (PDF_STRING);
  data = NEW (1, pdf_string);
  result -> data = data;
  if (length != 0) {
    data -> length = length;
    data -> string = NEW (length+1, unsigned char);
    memcpy (data -> string, string, length);
    data -> string[length] = 0;
  } else {
    data -> length = 0;
    data -> string = NULL;
  }
  return result;
}

unsigned char *pdf_string_value (pdf_obj *a_pdf_string)
{
  pdf_string *data;
  data = a_pdf_string -> data;
  return data -> string;
}

int pdfobj_escape_c (char *buffer, unsigned char ch)
{
  switch (ch) {
  case '(':
    return sprintf (buffer, "\\(");
  case ')':
    return sprintf (buffer, "\\)");
  case '\\':
    return sprintf (buffer, "\\\\");
  case '\n':
    return sprintf (buffer, "\\n");
  case '\r':
    return sprintf (buffer, "\\r");
  case '\t':
    return sprintf (buffer, "\\t");
  case '\b':
    return sprintf (buffer, "\\b");
  case '\f':
    return sprintf (buffer, "\\f");
  default:
    if (!isprint (ch)) {
      return sprintf (buffer, "\\%03o", ch);
    } else {
      return sprintf (buffer, "%c", ch);
    }
  }
}


static void write_string (FILE *file, const pdf_string *string)
{
  unsigned char *s = string -> string;
  int i, count;
  pdf_out_char (file, '(');
  for (i=0; i< string -> length; i++) {
    count = pdfobj_escape_c (format_buffer, s[i]);
    pdf_out (file, format_buffer, count);
  }
  pdf_out_char (file, ')');
}

static void release_string (pdf_string *data)
{
  if (data -> string != NULL)
    free (data -> string);
  free (data);
}

void pdf_set_string (pdf_obj *object, unsigned char *string, unsigned length)
{
  pdf_string *data;
  if (object == NULL || object -> type != PDF_STRING) {
     ERROR ("pdf_set_string:  Passed non-string object");
  }
  data = object -> data;
  if (data -> length != 0) {
    free (data -> string);
  }
  if (length != 0) {
    data -> length = length;
    data -> string = NEW (length, unsigned char);
    strncpy (data -> string, string, length);
  } else {
    data -> length = 0;
    data -> string = NULL;
  }
}

int pdf_check_name(const char *name)
{
  static char *valid_chars =
    "!\"&'*+,-.0123456789:;=?@ABCDEFGHIJKLMNOPQRSTUVWXYZ\\^_`abcdefghijklmnopqrstuvwxyz|~";
  if (strspn (name, valid_chars) == strlen (name))
    return 1;
  else
    return 0;
}

pdf_obj *pdf_new_name (const char *name)  /* name does *not* include the / */ 
{
  pdf_obj *result;
  unsigned length = strlen (name);
  pdf_name *data;
  if (!pdf_check_name (name)) {
    fprintf (stderr, "%s\n", name);
    ERROR ("pdf_new_name:  invalid PDF name");
  }
  result = pdf_new_obj (PDF_NAME);
  data = NEW (1, pdf_name);
  result -> data = data;
  if (length != 0) {
    data -> name = NEW (length+1, char);
    strncpy (data -> name, name, length+1);
  } else 
    data -> name = NULL;
  return result;
}

int pdf_match_name (const pdf_obj *name_obj, const char *name_string)
{
  pdf_name *data;
  data = name_obj -> data;
  return (!strcmp (data -> name, name_string));
}


static void write_name (FILE *file, const pdf_name *name)
{
  char *s = name -> name;
  int i, length;
  pdf_out_char (file, '/');
  if (name -> name == NULL)
    length = 0;
  else
    length = strlen (name -> name);
  for (i=0; i < length; i++) {
    if (isprint (s[i]) &&
	s[i] != '/' &&
	s[i] != '%' &&
	s[i] != '(' &&
	s[i] != ')' &&
	s[i] != '[' && 
	s[i] != ']' && 
	s[i] != '#')
      pdf_out_char (file, s[i]);
  }
}


static void release_name (pdf_name *data)
{
  if (data -> name != NULL)
    free (data -> name);
  free (data);
}

void pdf_set_name (pdf_obj *object, char *name)
{
  pdf_name *data;
  unsigned length = strlen (name);
  if (object == NULL || object -> type != PDF_NAME) {
     ERROR ("pdf_set_name:  Passed non-name object");
  }
  data = object -> data;
  if (data -> name != NULL) {
    free (data -> name);
  }
  if (length != 0) {
    data -> name = NEW (length, char);
    strncpy (data -> name, name, length);
  } else {
    data -> name = NULL;
  }
}

char *pdf_name_value (pdf_obj *object)
{
  pdf_name *data;
  if (object == NULL || object -> type != PDF_NAME) {
     ERROR ("pdf_name_value:  Passed non-name object");
  }
  data = object -> data;
  if (data -> name == NULL)
    return NULL;
  return data -> name;
}


pdf_obj *pdf_new_array (void)
{
  pdf_obj *result;
  pdf_array *data;
  result = pdf_new_obj (PDF_ARRAY);
  data = NEW (1, pdf_array);
  data -> this = NULL;
  data -> next = NULL;
  result -> data = data;
  return result;
}

static void write_array (FILE *file, const pdf_array *array)
{
  if (array -> next == NULL) {
    write_null (file);
    return;
  }
  pdf_out_char (file, '[');
  while (array -> next != NULL) {
    pdf_write_obj (file, array -> this);
    array = array -> next;
    if (array -> next != NULL)
      pdf_out_white (file);
  }
  pdf_out_char (file, ']');
}

pdf_obj *pdf_get_array (pdf_obj *array, int index)
{
  pdf_array *data;
  if (array == NULL) {
    ERROR ("pdf_get_array: passed NULL object");
  }
  if (array -> type != PDF_ARRAY) {
    ERROR ("pdf_get_array: passed non array object");
  }
  data = array -> data;
  while (--index > 0 && data -> next != NULL)
    data = data -> next;
  if (data -> next == NULL) {
    return NULL;
  }
  return data -> this;
}



static void release_array (pdf_array *data)
{
  pdf_array *next;
  while (data != NULL && data -> next != NULL) {
    pdf_release_obj (data -> this);
    next = data -> next;
    free (data);
    data = next;
  }
  free (data);
}

void pdf_add_array (pdf_obj *array, pdf_obj *object) /* Array is ended
							by a node with NULL
							this pointer */
{
  pdf_array *data;
  pdf_array *new_node;
  if (array == NULL || array -> type != PDF_ARRAY) {
     ERROR ("pdf_add_array:  Passed non-array object");
  }
  data = array -> data;
  new_node = NEW (1, pdf_array);
  new_node -> this = NULL;
  new_node -> next = NULL;
  while (data -> next != NULL)
    data = data -> next;
  data -> next = new_node;
  data -> this = object;
}


static void write_dict (FILE *file, const pdf_dict *dict)
{
  pdf_out (file, "<<\n", 3);
  while (dict -> key != NULL) {
    pdf_write_obj (file, dict -> key);
    pdf_out_white (file);
    pdf_write_obj (file, dict -> value);
    dict = dict -> next;
    pdf_out_char (file, '\n');
  }
  pdf_out (file, ">>", 2);
}

pdf_obj *pdf_new_dict (void)
{
  pdf_obj *result;
  pdf_dict *data;
  result = pdf_new_obj (PDF_DICT);
  data = NEW (1, pdf_dict);
  data -> key = NULL;
  data -> value = NULL;
  data -> next = NULL;
  result -> data = data;
  return result;
}

static void release_dict (pdf_dict *data)
{
  pdf_dict *next;
  while (data != NULL && data -> key != NULL) {
    pdf_release_obj (data -> key);
    pdf_release_obj (data -> value);
    next = data -> next;
    free (data);
    data = next;
  }
  free (data);
}

void pdf_add_dict (pdf_obj *dict, pdf_obj *key, pdf_obj *value) /* Array is ended
								   by a node with NULL
								   this pointer */
{
  pdf_dict *data;
  pdf_dict *new_node;
  if (dict == NULL || dict -> type != PDF_DICT) {
     ERROR ("pdf_add_dict:  Passed non-dict object");
  }
  if (key == NULL || key -> type != PDF_NAME ) {
    ERROR ("pdf_add_dict: Passed invalid key");
  }
  if (value != NULL && (value -> type == 0 ||
      value -> type > PDF_INDIRECT )) {
    ERROR ("pdf_add_dict: Passed invalid value");
  }
  data = dict -> data;
  new_node = NEW (1, pdf_dict);
  new_node -> key = NULL;
  new_node -> value = NULL;
  new_node -> next = NULL;
  while (data -> key != NULL)
    data = data -> next;
  data -> next = new_node;
  data -> key = key;
  data -> value = value;
}

/* pdf_merge_dict makes a link for each item in dict2 before
   stealing it */
void pdf_merge_dict (pdf_obj *dict1, pdf_obj *dict2)
{
  pdf_dict *data;
  if (dict1 == NULL || dict1 -> type != PDF_DICT) 
    ERROR ("pdf_merge_dict:  Passed invalid first dictionary");
  if (dict2 == NULL || dict2 -> type != PDF_DICT)
    ERROR ("pdf_merge_dict:  Passed invalid second dictionary");
  data = dict2 -> data;
  while (data -> key != NULL) {
    pdf_name *key = (data -> key) -> data;
    if (key == NULL || pdf_lookup_dict (dict1, key -> name) == NULL) {
      pdf_add_dict (dict1, pdf_link_obj(data -> key),
		    pdf_link_obj (data -> value));
    }
    data = data -> next;
  }
}

pdf_obj *pdf_lookup_dict (const pdf_obj *dict, const char *name)
{
  pdf_dict *data;
  if (dict == NULL || dict ->type != PDF_DICT) 
    ERROR ("pdf_lookup_dict:  Passed invalid dictionary");
  data = dict -> data;
  while (data -> key != NULL) {
    if (pdf_match_name (data -> key, name))
      return data -> value;
    data = data -> next;
  }
  return NULL;
}

char *pdf_get_dict (const pdf_obj *dict, int index)
{
  pdf_dict *data;
  char *result;
  if (dict == NULL) {
    ERROR ("pdf_get_dict: passed NULL object");
  }
  if (dict -> type != PDF_DICT) {
    ERROR ("pdf_get_dict: passed non array object");
  }
  data = dict -> data;
  while (--index > 0 && data -> next != NULL)
    data = data -> next;
  if (data -> next == NULL)
    return NULL;
  result = pdf_name_value (data -> key);
  return result;
}


pdf_obj *pdf_new_stream (void)
{
  pdf_obj *result;
  pdf_stream *data;
  result = pdf_new_obj (PDF_STREAM);
  data = NEW (1, pdf_stream);
  result -> data = data;
  data -> dict = pdf_new_dict ();  /* Although we are using an arbitrary
				      pdf_object here, it must have
				      type=PDF_DICT and cannot be an
				      indirect reference.  This will
				      be checked by the output routine 
				   */
  data -> length = pdf_new_number (0);
  pdf_add_dict (data->dict,
		pdf_new_name ("Length"),
		pdf_ref_obj (data -> length));
  if ((data -> tmpfile = tmpfile()) == NULL) {
    ERROR ("pdf_new_stream:  Could not create temporary file");
  };
  return result;
}

static void write_stream (FILE *file, const pdf_stream *stream)
{
  int length = 0, nwritten = 0, lastchar = 0;
  pdf_write_obj (file, stream -> dict);
  pdf_out (file, "\nstream\n", 8);
  rewind (stream -> tmpfile);
  while ((nwritten = fread (work_buffer, sizeof (char),
			    WORK_BUFFER_SIZE,
			    stream -> tmpfile)) > 0) {
    pdf_out (file, work_buffer, nwritten);
    length += nwritten;
    lastchar = work_buffer[nwritten-1];
  }
  /* If stream doesn't have an eol, put one there */
  if (lastchar != '\n') {
    pdf_out (file, "\n", 1);
    length +=1;
  }
  pdf_set_number (stream -> length, length);
  pdf_out (file, "endstream", 9);
}


static void release_stream (pdf_stream *stream)
{
  pdf_release_obj (stream -> dict);
  pdf_release_obj (stream -> length);
  fclose (stream -> tmpfile);
}

pdf_obj *pdf_stream_dict (pdf_obj *stream)
{
  pdf_stream *data;
  if (stream == NULL || stream -> type != PDF_STREAM) {
     ERROR ("pdf_stream_dict:  Passed non-stream object");
  }
  data = stream -> data;
  return data -> dict;
}

void pdf_add_stream (pdf_obj *stream, char *stream_data, unsigned length)
{
  pdf_stream *data;

  if (stream == NULL || stream -> type != PDF_STREAM) {
     ERROR ("pdf_add_stream:  Passed non-stream object");
  }
  if (length == 0)
    return;
  data = stream -> data;
  fwrite (stream_data, 1, length, data -> tmpfile);
}

void pdf_write_obj (FILE *file, const pdf_obj *object)
{
  if (object == NULL) {
    ERROR ("pdf_write_obj passed null pointer");
  }
  if (object -> type > PDF_INDIRECT) {
    ERROR ("pdf_write_obj:  Called with invalid object");
  }
  if (file == stderr)
    fprintf (stderr, "{%d}", object -> refcount);
  switch (object -> type) {
  case PDF_BOOLEAN:
    write_boolean (file, object -> data);
    break;
  case PDF_NUMBER:
    write_number (file, object -> data);
    break;
  case PDF_STRING:
    write_string (file, object -> data);
    break;
  case PDF_NAME:
    write_name (file, object -> data);
    break;
  case PDF_ARRAY:
    write_array (file, object -> data);
    break;
  case PDF_DICT:
    write_dict (file, object -> data);
    break;
  case PDF_STREAM:
    write_stream (file, object -> data);
    break;
  case PDF_NULL:
    write_null (file);
    break;
  case PDF_INDIRECT:
    write_indirect (file, object -> data);
    break;
  }
}

static void pdf_flush_obj (FILE *file, const pdf_obj *object) 
     /* Write the object to the file */ 
{
  int length;
  /* Record file position.  No object is numbered 0, so subtract 1
     when using as an array index */
  output_xref[object->label-1].file_position = pdf_output_file_position;
  length = sprintf (format_buffer, "%d %d obj\n", object -> label ,
		    object -> generation);
  pdf_out (file, format_buffer, length);
  pdf_write_obj (file, object);
  pdf_out (file, "\nendobj\n", 8);
}


void pdf_release_obj (pdf_obj *object)
{
  if (object == NULL)
    return;
  if (object -> type > PDF_INDIRECT ||
      object -> refcount == 0) {
    fprintf (stderr, "pdf_release_obj: object = %p, type = %d\n", object, object ->
	     type);
    
    ERROR ("pdf_release_obj:  Called with invalid object");
  }
  object -> refcount -= 1;
    if (object -> refcount == 0) { /* Nothing is using this object so it's okay to
				    remove it */
    /* Nonzero "label" means object needs to be written before it's destroyed*/
    if (object -> label && pdf_output_file != NULL) { 
      pdf_flush_obj (pdf_output_file, object);
    }
    switch (object -> type) {
    case PDF_BOOLEAN:
      release_boolean (object -> data);
      break;
    case PDF_NULL:
      release_null (object -> data);
      break;
    case PDF_NUMBER:
      release_number (object -> data);
      break;
    case PDF_STRING:
      release_string (object -> data);
      break;
    case PDF_NAME:
      release_name (object -> data);
      break;
    case PDF_ARRAY:
      release_array (object -> data);
      break;
    case PDF_DICT:
      release_dict (object -> data);
      break;
    case PDF_STREAM:
      release_stream (object -> data);
      break;
    case PDF_INDIRECT:
      release_indirect (object -> data);
      break;
    }
    object -> type = -1;  /* This might help detect freeing already freed objects */
    free (object);
  }
}

static int backup_line (void)
{
  int ch;
  ch = 0;
  if (debug) {
    fprintf (stderr, "\nbackup_line:\n");
  }
  /* Note: this code should work even if \r\n is eol.
     It could fail on a machine where \n is eol and
     there is a \r in the stream---Highly unlikely
     in the last few bytes where this is likely to be used.
  */
  do {
    seek_relative (pdf_input_file, -2);
    if (debug)
      fprintf (stderr, "%c", ch);
  } while (tell_position (pdf_input_file) > 0 &&
	   (ch = fgetc (pdf_input_file)) >= 0 &&
	   (ch != '\n' && ch != '\r' ));
  if (debug)
    fprintf (stderr, "<-\n");
  if (ch < 0) {
    fprintf (stderr, "Invalid PDF file\n");
    return 1;
  }
  return 0;
}

static pdf_file_size = 0;

static long find_xref(void)
{
  long currentpos, xref_pos;
  int tries = 0;
  char *start, *end, *number;
  if (debug)
    fprintf (stderr, "(find_xref");
  seek_end (pdf_input_file);
  pdf_file_size = tell_position (pdf_input_file);
  do {
    tries ++;
    backup_line();
    currentpos = tell_position(pdf_input_file);
    fread (work_buffer, sizeof(char), strlen("startxref"),
	   pdf_input_file);
    if (debug) {
      work_buffer[strlen("startxref")] = 0;
      fprintf (stderr, "[%s]\n", work_buffer);
    }
    seek_absolute(pdf_input_file, currentpos);
  } while (tries < 10 && strncmp (work_buffer, "startxref", strlen ("startxref")));
  if (tries >= 10)
    return 0;
  /* Skip rest of this line */
  mfgets (work_buffer, WORK_BUFFER_SIZE, pdf_input_file);
  /* Next line of input file should contain actual xref location */
  mfgets (work_buffer, WORK_BUFFER_SIZE, pdf_input_file);
  if (debug) {
    fprintf (stderr, "\n->[%s]<-\n", work_buffer);
  }
  start = work_buffer;
  end = start+strlen(work_buffer);
  skip_white(&start, end);
  xref_pos = (long) atof (number = parse_number (&start, end));
  release (number);
  if (debug) {
    fprintf (stderr, ")\n");
    fprintf (stderr, "xref @ %ld\n", xref_pos);
  }
  return xref_pos;
}

pdf_obj *parse_trailer (void)
{
  char *start;
  /* This routine must be called with the file pointer located at
     the start of the trailer */
  /* Fill work_buffer and hope trailer fits.  This should
     be made a bit more robust sometime */
  if (fread (work_buffer, sizeof(char), WORK_BUFFER_SIZE,
	     pdf_input_file) == 0 ||
      strncmp (work_buffer, "trailer", strlen("trailer"))) {
    fprintf (stderr, "No trailer.  Are you sure this is a PDF file?\n");
    fprintf (stderr, "\nbuffer:\n->%s<-\n", work_buffer);
    return NULL;
  }
  start = work_buffer + strlen("trailer");
  skip_white(&start, work_buffer+WORK_BUFFER_SIZE);
  return (parse_pdf_dict (&start, work_buffer+WORK_BUFFER_SIZE));
}

struct object 
{
  unsigned long position;
  unsigned generation;
  /* Object numbers in original file and new file must be different
     new_ref provides a reference for the object in the new file
     object space.  When it is first set, an object in the old file
     is copied to the new file with a new number.  new_ref remains set
     until the file is closed so that future references can access the
     object via new_ref instead of copying the object again */
  pdf_obj *direct;
  pdf_obj *indirect;
  int used;
} *xref_table = NULL;
long num_input_objects;

long next_object (int obj)
{
  /* routine tries to estimate an upper bound for character position
     of the end of the object, so it knows how big the buffer must be.
     The parsing routines require that the entire object be read into
     memory. It would be a major pain to rewrite them.  The worst case
     is that an object before an xref table will grab the whole table
     :-( */
  int i;
  long this_position, result = pdf_file_size;  /* Worst case */
  this_position = xref_table[obj].position;
  /* Check all other objects to find next one */
  for (i=0; i<num_input_objects; i++) {
    if (xref_table[i].position > this_position &&
	xref_table[i].position < result)
      result = xref_table[i].position;
  }
  return result;
}

/* The following routine returns a reference to an object existing
   only in the input file.  It does this as follows.  If the object
   has never been referenced before, it reads the object
   in and creates a reference to it.  Then it writes
   the object out, keeping the existing reference. If the
   object has been read in (and written out) before, it simply
   returns the retained existing reference to that object */

pdf_obj *pdf_ref_file_obj (int obj_no)
{
  pdf_obj *direct, *indirect;
  if (obj_no < 0 || obj_no >= num_input_objects) {
    fprintf (stderr, "\n\npdf_ref_file_obj: nonexistent object\n");
    return NULL;
  }
  if (xref_table[obj_no].indirect != NULL) {
    fprintf (stderr, "\npdf_ref_file_obj: obj=%d is already referenced\n",obj_no);
    return pdf_link_obj(xref_table[obj_no].indirect);
  }
  if ((direct = pdf_read_object (obj_no)) == NULL) {
    fprintf (stderr, "\npdf_ref_file_obj: Could not read object\n");
    return NULL;
  }
  indirect = pdf_ref_obj (direct);
  xref_table[obj_no].indirect = indirect;
  xref_table[obj_no].direct = direct;
  /* Make sure the caller can't doesn't free this object */
  return pdf_link_obj(indirect);
}


pdf_obj *pdf_new_ref (int label, int generation) 
{
  pdf_obj *result;
  pdf_indirect *indirect;
  if (label >= num_input_objects || label < 0) {
    fprintf (stderr, "pdf_new_ref: Object doesn't exist\n");
    return NULL;
  }
  result = pdf_new_obj (PDF_INDIRECT);
  indirect = NEW (1, pdf_indirect);
  result -> data = indirect;
  indirect -> label = label;
  indirect -> generation = generation;
  indirect -> dirty = 1;
  indirect -> dirty_file = pdf_input_file;
  return result;
}

pdf_obj *pdf_read_object (int obj_no) 
{
  long start_pos, end_pos;
  char *buffer, *number, *parse_pointer, *end;
  pdf_obj *result;
  if (debug) {
    fprintf (stderr, "\nread_object: obj=%d\n", obj_no);
  }
  if (obj_no < 0 || obj_no >= num_input_objects) {
    fprintf (stderr, "\nTrying to read nonexistent object\n");
    return NULL;
  }
  if (!xref_table[obj_no].used) {
    fprintf (stderr, "\nTrying to read deleted object\n");
    return NULL;
  }
  if (debug) {
    fprintf (stderr, "\nobj@%ld\n", xref_table[obj_no].position);
  }
  seek_absolute (pdf_input_file, start_pos =
		 xref_table[obj_no].position);
  end_pos = next_object (obj_no);
  if (debug) {
    fprintf (stderr, "\nendobj@%ld\n", end_pos);
  }
  buffer = NEW (end_pos - start_pos+1, char);
  fread (buffer, sizeof(char), end_pos-start_pos, pdf_input_file);
  buffer[end_pos-start_pos] = 0;
  if (debug) {
    fprintf (stderr, "\nobject:\n%s", buffer);
  }
  parse_pointer = buffer;
  end = buffer+(end_pos-start_pos);
  skip_white (&parse_pointer, end);
  number = parse_number (&parse_pointer, end);
  if ((int) atof(number) != obj_no) {
    fprintf (stderr, "Object number doesn't match\n");
    release (buffer);
    return NULL;
  }
  if (number != NULL)
    release(number);
  skip_white (&parse_pointer, end);
  number = parse_number (&parse_pointer, end);
  if (number != NULL)
    release(number);
  skip_white(&parse_pointer, end);
  if (strncmp(parse_pointer, "obj", strlen("obj"))) {
    fprintf (stderr, "Didn't find \"obj\"\n");
    release (buffer);
    return (NULL);
  }
  parse_pointer += strlen("obj");
  result = parse_pdf_object (&parse_pointer, end);
  skip_white (&parse_pointer, end);
  if (strncmp(parse_pointer, "endobj", strlen("endobj"))) {
    fprintf (stderr, "Didn't find \"endobj\"\n");
    if (result != NULL)
      pdf_release_obj (result);
    result = NULL;
  }
  release (buffer);
  return (result);
}
/* pdf_deref_obj always returns a link instead of the original */ 
pdf_obj *pdf_deref_obj (pdf_obj *obj)
{
  pdf_obj *result, *tmp;
  pdf_indirect *indirect;
  if (obj == NULL)
    return NULL;
  if (obj -> type != PDF_INDIRECT) {
    return pdf_link_obj (obj);
  }
  indirect = obj -> data;
  if (!(indirect -> dirty)) {
    ERROR ("Tried to deref a non-file object");
  }
  result = pdf_read_object (indirect -> label);
if (debug){
  fprintf (stderr, "\npdf_deref_obj: read_object returned\n");
  pdf_write_obj (stderr, result);
  }
 
  while (result -> type == PDF_INDIRECT) {
    tmp = pdf_read_object (result -> label);
    pdf_release_obj (result);
    result = tmp;
  }
  return result;
}

/* extends the xref table if we get another segment
   with higher object numbers than the current object */
static void extend_xref (long new_size) 
{
  int i;
  xref_table = RENEW (xref_table, new_size,
		      struct object);
  for (i=num_input_objects; i<new_size; i++) {
    xref_table[i].direct = NULL;
    xref_table[i].indirect = NULL;
    xref_table[i].used = 0;
  }
  num_input_objects = new_size;
}



static int parse_xref (void)
{
  long first_obj, num_table_objects;
  long i;
  /* This routine reads one xref segment.  It must be called
     positioned at the beginning of an xref table.  It may be called
     multiple times on the same file.  xref tables sometimes come in
     pieces */
  mfgets (work_buffer, WORK_BUFFER_SIZE, pdf_input_file);
  if (strncmp (work_buffer, "xref", strlen("xref"))) {
    fprintf (stderr, "No xref.  Are you sure this is a PDF file?\n");
    return 0;
  }
  /* Next line in file has first item and size of table */
  mfgets (work_buffer, WORK_BUFFER_SIZE, pdf_input_file);
  sscanf (work_buffer, "%ld %ld", &first_obj, &num_table_objects);
  if (num_input_objects < first_obj+num_table_objects) {
    extend_xref (first_obj+num_table_objects);
  }
  if (debug) {
    fprintf (stderr, "\nfirstobj=%ld,number=%ld\n",
	     first_obj,num_table_objects);
  }
  
  for (i=first_obj; i<first_obj+num_table_objects; i++) {
    fread (work_buffer, sizeof(char), 20, pdf_input_file);
    work_buffer[19] = 0;
    sscanf (work_buffer, "%ld %d", &(xref_table[i].position), 
	    &(xref_table[i].generation));
    if (0) {
      fprintf (stderr, "pos: %ld gen: %d\n", xref_table[i].position,
	       xref_table[i].generation);
    }
    if (work_buffer[17] != 'n' && work_buffer[17] != 'f') {
      fprintf (stderr, "PDF file is corrupt\n");
      fprintf (stderr, "[%s]\n", work_buffer);
      return 0;
    }
    if (work_buffer[17] == 'n')
      xref_table[i].used = 1;
    else
      xref_table[i].used = 0;
    xref_table[i].direct = NULL;
    xref_table[i].indirect = NULL;
  }
  return 1;
}


pdf_obj *read_xref (void)
{
  pdf_obj *main_trailer, *prev_trailer, *prev_xref, *xref_size;
  long xref_pos;
  if ((xref_pos = find_xref()) == 0) {
    fprintf (stderr, "No xref loc.  Is this a correct PDF file?\n");
    return NULL;
  }
  if (debug) {
    fprintf(stderr, "xref@%ld\n", xref_pos);
  }
  /* Read primary xref table */
  seek_absolute (pdf_input_file, xref_pos);
  if (!parse_xref()) {
    fprintf (stderr,
	     "\nCouldn't read xref table.  Is this a correct PDF file?\n");
    return NULL;
  }
  if ((main_trailer = parse_trailer()) == NULL) {
    fprintf (stderr,
	     "\nCouldn't read xref trailer.  Is this a correct PDF file?\n");
    return NULL;
  }
  if (debug) {
    fprintf (stderr, "\nmain trailer:\n");
    pdf_write_obj (stderr, main_trailer);
  }
  if (pdf_lookup_dict (main_trailer, "Root") == NULL ||
      (xref_size = pdf_lookup_dict (main_trailer, "Size")) == NULL) {
    fprintf (stderr,
	     "\nTrailer doesn't have catalog or a size.  Is this a correct PDF file?\n");
    return NULL;
  }
  if (num_input_objects < pdf_number_value (xref_size)) {
    extend_xref (pdf_number_value (xref_size));
  }
  /* Read any additional xref tables */
  prev_trailer = pdf_link_obj (main_trailer);
  while ((prev_xref = pdf_lookup_dict (prev_trailer, "Prev")) != NULL) {
    pdf_release_obj (prev_trailer);
    xref_pos = pdf_number_value (prev_xref);
    seek_absolute (pdf_input_file, xref_pos);
    if (!parse_xref()) {
      fprintf (stderr,
	       "\nCouldn't read xref table.  Is this a correct PDF file?\n");
      return NULL;
    }
    if ((prev_trailer = parse_trailer()) == NULL) {
      fprintf (stderr,
	       "\nCouldn't read xref trailer.  Is this a correct PDF file?\n");
      return NULL;
    }
    if (debug) {
      fprintf (stderr, "\nprev_trailer:\n");
      pdf_write_obj (stderr, prev_trailer);
    }
  }
  return main_trailer;
}

pdf_obj *pdf_open (char *filename)
{
  pdf_obj *trailer;
  if ((pdf_input_file = fopen (filename, FOPEN_RBIN_MODE)) == NULL) {
    fprintf (stderr, "Unable to open file name (%s)\n", filename);
    return NULL;
  }
  if (!check_for_pdf (pdf_input_file)) {
    fprintf (stderr, "pdf_open: %s: not a PDF 1.[1-2] file\n", filename);
    return NULL;
  }
  if ((trailer = read_xref()) == NULL) {
    fprintf (stderr, "\nProbably not a PDF file\n");
    pdf_close ();
    return NULL;
  }
  if (debug) {
    fprintf (stderr, "\nDone with xref:\n");
  }
  if (debug)
    pdf_write_obj (stderr, trailer);
  return trailer;
}

void pdf_close (void)
{
  /* Following loop must be iterated because each write could trigger
     an additional indirect reference of an object with a lower
     number! */
  int i, done;
  if (debug) {
    fprintf (stderr, "\npdf_close:\n");
    fprintf (stderr, "pdf_input_file=%p\n", pdf_input_file);
  }
  
  do {
    done = 1;
    for (i=0; i<num_input_objects; i++) {
      if (xref_table[i].direct != NULL) {
	pdf_release_obj (xref_table[i].direct);
	xref_table[i].direct = NULL;
	done = 0;
      }
    }
  } while (!done);
  release (xref_table);
  xref_table = NULL;
  num_input_objects = 0;
  fclose (pdf_input_file);
  pdf_input_file = NULL;
  if (debug) {
    fprintf (stderr, "\nexiting pdf_close:\n");
  }
}

int check_for_pdf (FILE *file) 
{
  rewind (file);
  if (fread (work_buffer, sizeof(char), strlen("%PDF-1.2"), file) != 8 ||
      strncmp(work_buffer, "%PDF-1.", 7) ||
      work_buffer[7] < '0' || work_buffer[7] > '2')
    return 0;
  return 1;
}

