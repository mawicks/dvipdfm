#include <ctype.h>
#include <string.h>

#include "pdfobj.h"
#include "mem.h"
#include "error.h"


unsigned next_label = 1;
FILE *pdf_file = NULL;
unsigned long file_position = 0;
char format_buffer[256];

static struct xref_entry 
{
  unsigned long file_position;
} xref[PDF_MAX_IND_OBJECTS];
static unsigned startxref;

static unsigned pdf_root_obj = 0, pdf_info_obj = 0;

/* Internal static routines */

static void pdf_write_obj (const pdf_obj *object);
static void pdf_flush_obj (const pdf_obj *object);
static void pdf_label_obj (pdf_obj *object);
static void pdf_out_char (char c);
static void pdf_out (char *buffer, int length);
static void release_indirect (struct pdf_indirect *data);
static void write_indirect (const struct pdf_indirect *indirect);
static void release_boolean (struct pdf_obj *data);
static void write_boolean (const struct pdf_boolean *data);
static void release_number (struct pdf_number *data);
static void write_number (const struct pdf_number *number);
static void write_string (const struct pdf_string *string);
static void release_string (struct pdf_string *data);
static void write_name (const struct pdf_name *name);
static void release_name (struct pdf_name *data);
static void write_array (const struct pdf_array *array);
static void release_array (struct pdf_array *data);
static void write_dict (const struct pdf_dict *dict);
static void release_dict (struct pdf_dict *data);
static void write_stream (const struct pdf_stream *stream);
static pdf_obj *release_stream (struct pdf_stream *stream);

void pdf_out_init (const char *filename)
{
  if (!(pdf_file = fopen (filename, "w"))) {
    fprintf (stderr, "pdf_out_init: %s\n");
    ERROR ("pdf_init:  Could not open file");
  }
  pdf_out ("%PDF-1.2\n", 9);
}

static void dump_xref(void)
{
  int length, i;
  startxref = file_position;	/* Record where this xref is for
				   trailer */
  pdf_out ("xref\n", 5);
  length = sprintf (format_buffer, "%d %d\n", 0, next_label);
  pdf_out (format_buffer, length);
  length = sprintf (format_buffer, "%010ld %05ld f \n", 0L, 65535L);
  /* Every space counts.  The space after the 'f' and 'n' is
   *essential*.  The PDF spec says the lines must be 20 characters
   long including the end of line character. */
  pdf_out (format_buffer, length);
  for (i=1; i<next_label; i++){
    length = sprintf (format_buffer, "%010ld %05ld n \n",
		      xref[i-1].file_position, 0L);
    pdf_out (format_buffer, length);
  }
}

static void dump_trailer(void)
{
  int length;
  unsigned long starttrailer;
  starttrailer = file_position;
  pdf_out ("trailer\n", 8);
  pdf_out ("<<\n", 3);
  length = sprintf (format_buffer, "/Size %ld\n",
		    next_label);
  pdf_out (format_buffer, length);
  if (pdf_root_obj == 0) 
    ERROR ("dump_trailer:  Invalid root object");
  length = sprintf (format_buffer, "/Root %u %u R\n", pdf_root_obj, 0);
  pdf_out (format_buffer, length);
  if (pdf_info_obj != 0) {
    length = sprintf (format_buffer, "/Info %u %u R\n", pdf_info_obj, 0);
    pdf_out (format_buffer, length);
  }
  pdf_out (">>\n", 3);
  pdf_out ("startxref\n", 10);
  length = sprintf (format_buffer, "%ld\n", startxref);
  pdf_out (format_buffer, length);
  pdf_out ("%%EOF\n", 6);
}



void pdf_out_flush (void)
{
  if (pdf_file) {
    dump_xref();
    dump_trailer();
    fclose (pdf_file);
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

static void pdf_out_char (char c)
{
  fputc (c, pdf_file);
  file_position += 1;
}

static void pdf_out (char *buffer, int length)
{
  fwrite (buffer, 1, length, pdf_file);
  file_position += length;
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
  if (next_label > PDF_MAX_IND_OBJECTS) {
    ERROR ("pdf_label_obj:  Indirect object capacity exceeded");
  }
  if (object -> label == 0) {  /* Don't change label on an already labeled
				  object.  Ignore such calls */
    object -> label = next_label++;
    object -> generation = 0;
  }
}

pdf_obj *pdf_ref_obj(pdf_obj *object)
{
  pdf_obj *result;
  struct pdf_indirect *indirect;
  
  if (object == NULL || object -> type == 0) {
    ERROR ("pdf_ref_obj:  Called with invalid object");
  }
  if (object -> label == 0) {
    pdf_label_obj (object);
  }
  result = pdf_new_obj (PDF_INDIRECT);
  indirect = NEW (1, struct pdf_indirect);
  result -> data = indirect;
  if (object -> type == PDF_INDIRECT) { /* If an object is already an indirect reference,
					   reference the original
					   object, not the indirect
					   one */
    indirect -> label = ((struct pdf_indirect *) (object -> data)) -> label;
    indirect -> generation = ((struct pdf_indirect *) (object -> data)) -> generation;
  } else {
    indirect -> label = object -> label;
    indirect -> generation = object -> generation;
  }
  return result;
}

static void release_indirect (struct pdf_indirect *data)
{
  free (data);
}

static void write_indirect (const struct pdf_indirect *indirect)
{
  int length;
  fprintf (stderr, "write_indirect: label: %d\n", indirect -> label);
  length = sprintf (format_buffer, "%d %d R", indirect -> label,
		    indirect -> generation);
  pdf_out (format_buffer, length);
}

pdf_obj *pdf_new_boolean (char value)
{
  pdf_obj *result;
  struct pdf_boolean *data;
  result = pdf_new_obj (PDF_BOOLEAN);
  data = NEW (1, struct pdf_boolean);
  result -> data = data;
  data -> value = value;
  return result;
}

static void release_boolean (struct pdf_obj *data)
{
  free (data);
}

static void write_boolean (const struct pdf_boolean *data)
{
  if (data -> value) {
    pdf_out ("true", 4);
  }
  else {
    pdf_out ("false", 5);
  }
}


void pdf_set_boolean (pdf_obj *object, char value)
{
   if (object == NULL || object -> type != PDF_BOOLEAN) {
     ERROR ("pdf_set_boolean:  Passed non-boolean object");
   }
   ((struct pdf_boolean *) (object -> data)) -> value == value;
}

pdf_obj *pdf_new_number (double value)
{
  pdf_obj *result;
  struct pdf_number *data;
  result = pdf_new_obj (PDF_NUMBER);
  data = NEW (1, struct pdf_number);
  result -> data = data;
  data -> value = value;
  return result;
}

static void release_number (struct pdf_number *data)
{
  free (data);
}

static void write_number (const struct pdf_number *number)
{
  int count;
  count = sprintf (format_buffer, "%g", number -> value);
  pdf_out (format_buffer, count);
}


void pdf_set_number (pdf_obj *object, double value)
{
   if (object == NULL || object -> type != PDF_NUMBER) {
     ERROR ("pdf_set_number:  Passed non-number object");
   }
   ((struct pdf_number *) (object -> data)) -> value = value;
}

pdf_obj *pdf_new_string (const char *string, unsigned length)
{
  pdf_obj *result;
  struct pdf_string *data;
  result = pdf_new_obj (PDF_STRING);
  data = NEW (1, struct pdf_string);
  result -> data = data;
  if (length != 0) {
    data -> length = length;
    data -> string = NEW (length, char);
    bcopy (string, data -> string, length);
  } else {
    data -> length = 0;
    data -> string = NULL;
  }
  return result;
}

int pdfobj_escape_c (char *buffer, char ch)
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


static void write_string (const struct pdf_string *string)
{
  char *s = string -> string;
  int i, count;
  pdf_out_char ('(');
  for (i=0; i< string -> length; i++) {
    count = pdfobj_escape_c (format_buffer, s[i]);
    pdf_out (format_buffer, count);
  }
  pdf_out_char (')');
}

static void release_string (struct pdf_string *data)
{
  if (data -> string != NULL)
    free (data -> string);
  free (data);
}

void pdf_set_string (pdf_obj *object, char *string, unsigned length)
{
  struct pdf_string *data;
  if (object == NULL || object -> type != PDF_STRING) {
     ERROR ("pdf_set_string:  Passed non-string object");
  }
  data = object -> data;
  if (data -> length != 0) {
    free (data -> string);
  }
  if (length != 0) {
    data -> length = length;
    data -> string = NEW (length, char);
    strncpy (data -> string, string, length);
  } else {
    data -> length = 0;
    data -> string = NULL;
  }
}


pdf_obj *pdf_new_name (const char *name)  /* name does *not* include the / */ 
{
  pdf_obj *result;
  unsigned length = strlen (name);
  struct pdf_name *data;
  result = pdf_new_obj (PDF_NAME);
  data = NEW (1, struct pdf_name);
  result -> data = data;
  if (length != 0) {
    data -> name = NEW (length+1, char);
    strncpy (data -> name, name, length+1);
  } else 
    data -> name == NULL;
  return result;
}


static void write_name (const struct pdf_name *name)
{
  char *s = name -> name;
  int i, length;
  pdf_out_char ('/');
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
      pdf_out_char (s[i]);
  }
}


static void release_name (struct pdf_name *data)
{
  if (data -> name != NULL)
    free (data -> name);
  free (data);
}

void pdf_set_name (pdf_obj *object, char *name)
{
  struct pdf_name *data;
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

pdf_obj *pdf_new_array (void)
{
  pdf_obj *result;
  struct pdf_array *data;
  result = pdf_new_obj (PDF_ARRAY);
  data = NEW (1, struct pdf_array);
  data -> this = NULL;
  data -> next = NULL;
  result -> data = data;
  return result;
}

static void write_array (const struct pdf_array *array)
{
  pdf_out_char ('[');
  while (array -> this != NULL) {
    pdf_write_obj (array -> this);
    array = array -> next;
    if (array -> this != NULL)
      pdf_out_char (' ');
  }
  pdf_out_char (']');
}


static void release_array (struct pdf_array *data)
{
  struct pdf_array *next;
  fprintf (stderr, "release_array:\n");
  
  while (data -> this != NULL) {
    pdf_release_obj (data -> this);
    next = data -> next;
    free (data);
    data = next;
  }
  free (next);
}

void pdf_add_array (pdf_obj *array, pdf_obj *object) /* Array is ended
							by a node with NULL
							this pointer */
{
  struct pdf_array *data;
  struct pdf_array *new_node;
  fprintf (stderr, "pdf_add_array\n");
  if (array == NULL || array -> type != PDF_ARRAY) {
     ERROR ("pdf_add_array:  Passed non-array object");
  }
  if (object == NULL) {
    ERROR ("pdf_add_array: Passed non-object");
  }
  data = array -> data;
  new_node = NEW (1, struct pdf_array);
  new_node -> this = NULL;
  new_node -> next = NULL;
  while (data -> this != NULL)
    data = data -> next;
  data -> next = new_node;
  data -> this = object;
  object -> refcount += 1;  /* Increment ref count */
}


static void write_dict (const struct pdf_dict *dict)
{
  pdf_out ("<<\n", 3);
  while (dict -> key != NULL) {
    pdf_write_obj (dict -> key);
    pdf_out_char (' ');
    pdf_write_obj (dict -> value);
    dict = dict -> next;
    pdf_out_char ('\n');
  }
  pdf_out (">>", 2);
}

pdf_obj *pdf_new_dict (void)
{
  pdf_obj *result;
  struct pdf_dict *data;
  result = pdf_new_obj (PDF_DICT);
  data = NEW (1, struct pdf_dict);
  data -> key = NULL;
  data -> value = NULL;
  data -> next = NULL;
  result -> data = data;
  return result;
}

static void release_dict (struct pdf_dict *data)
{
  struct pdf_dict *next;
  while (data -> key != NULL) {
    pdf_release_obj (data -> key);
    pdf_release_obj (data -> value);
    next = data -> next;
    free (data);
    data = next;
  }
  free (next);
}

void pdf_add_dict (pdf_obj *dict, pdf_obj *key, pdf_obj *value) /* Array is ended
								   by a node with NULL
								   this pointer */
{
  struct pdf_dict *data;
  struct pdf_dict *new_node;
  if (dict == NULL || dict -> type != PDF_DICT) {
     ERROR ("pdf_add_dict:  Passed non-dict object");
  }
  if (key == NULL || key -> type != PDF_NAME ) {
    ERROR ("pdf_add_dict: Passed invalid key");
  }
  if (value == NULL || value -> type == 0 ||
      value -> type > PDF_INDIRECT ) {
    ERROR ("pdf_add_dict: Passed invalid value");
  }
  data = dict -> data;
  new_node = NEW (1, struct pdf_dict);
  new_node -> key = NULL;
  new_node -> value = NULL;
  new_node -> next = NULL;
  while (data -> key != NULL)
    data = data -> next;
  data -> next = new_node;
  data -> key = key;
  data -> value = value;
  key -> refcount += 1;  /* Increment ref count */
  value -> refcount += 1; 
}

pdf_obj *pdf_new_stream (void)
{
  pdf_obj *result, *tmp1, *tmp2;
  struct pdf_stream *data;
  result = pdf_new_obj (PDF_STREAM);
  data = NEW (1, struct pdf_stream);
  result -> data = data;
  data -> dict = pdf_new_dict ();  /* Although we are using an arbitrary
				      pdf_object here, it must have
				      type=PDF_DICT and cannot be an
				      indirect reference.  This will
				      be checked by the output routine 
				   */
  data -> length = pdf_new_number (0);
  pdf_add_dict (data->dict,
		tmp1 = pdf_new_name ("Length"),
		tmp2 = pdf_ref_obj (data -> length));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  if ((data -> tmpfile = tmpfile()) == NULL) {
    ERROR ("pdf_new_stream:  Could not create temporary file");
  };
  return result;
}

static void write_stream (const struct pdf_stream *stream)
{
  int ch, length = 0;
  pdf_write_obj (stream -> dict);
  pdf_out ("\nstream\n", 8);
  rewind (stream -> tmpfile);
  while ((ch = fgetc (stream -> tmpfile)) >= 0) {
    pdf_out_char (ch);
    length += 1;
  }
  pdf_out ("\n", 1);
  pdf_set_number (stream -> length, length+1);
  pdf_out ("endstream", 9);
}


static pdf_obj *release_stream (struct pdf_stream *stream)
{
  pdf_release_obj (stream -> dict);
  pdf_release_obj (stream -> length);
  fclose (stream -> tmpfile);
}


void pdf_add_stream (pdf_obj *stream, char *stream_data, unsigned length)
{
  struct pdf_stream *data;

  if (stream == NULL || stream -> type != PDF_STREAM) {
     ERROR ("pdf_add_stream:  Passed non-stream object");
  }
  if (length == 0)
    return;
  data = stream -> data;
  fwrite (stream_data, 1, length, data -> tmpfile);
}

static void pdf_write_obj (const pdf_obj *object)
{
  int length;
  fprintf (stderr, "pdf_write_obj: type: %d\n", object -> type);
  if (object == NULL || object -> type > PDF_INDIRECT) {
    ERROR ("pdf_write_obj:  Called with invalid object");
  }
  switch (object -> type) {
  case PDF_BOOLEAN:
    write_boolean (object -> data);
    break;
  case PDF_NUMBER:
    write_number (object -> data);
    break;
  case PDF_STRING:
    write_string (object -> data);
    break;
  case PDF_NAME:
    write_name (object -> data);
    break;
  case PDF_ARRAY:
    write_array (object -> data);
    break;
  case PDF_DICT:
    write_dict (object -> data);
    break;
  case PDF_STREAM:
    write_stream (object -> data);
    break;
  case PDF_INDIRECT:
    write_indirect (object -> data);
    break;
  }
}

static void pdf_flush_obj (const pdf_obj *object) 
     /* Write the object to the file */ 
{
  int length;
  /* Record file position.  No object is numbered 0, so subtract 1
     when using as an array index */
  xref[object->label-1].file_position = file_position;
  length = sprintf (format_buffer, "%d %d obj\n", object -> label ,
		    object -> generation);
  pdf_out (format_buffer, length);
  pdf_write_obj (object);
  pdf_out ("\nendobj\n", 8);
}


void pdf_release_obj (pdf_obj *object)
{
  int length;
  if (object == NULL || object -> type > PDF_INDIRECT ||
      object -> refcount == 0) {
    fprintf (stderr, "pdf_release_obj: object = %p, type = %d\n", object, object ->
	     type);
    
    ERROR ("pdf_release_obj:  Called with invalid object");
  }
  object -> refcount -= 1;
  if (object -> refcount == 0) { /* Nothing is using this object so it's okay to
				    remove it */
    /* Nonzero "label" means object needs to be written before it's destroyed*/
    if (object -> label && pdf_file != NULL) { 
      pdf_flush_obj (object);
    }
    fprintf (stderr, "pdf_release_obj called with type = %d\n", object -> type);
  
    switch (object -> type) {
    case PDF_BOOLEAN:
      release_boolean (object -> data);
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
    }
    free (object);
  }
}
