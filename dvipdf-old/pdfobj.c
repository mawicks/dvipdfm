#include <ctype.h>
#include <string.h>

#include "pdfobj.h"
#include "mem.h"
#include "error.h"
#include "io.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

unsigned next_label = 1;
FILE *pdf_file = NULL;
unsigned long file_position = 0;
char format_buffer[256];

static struct xref_entry 
{
  unsigned long file_position;
  pdf_obj *pdf_obj;
} xref[PDF_MAX_IND_OBJECTS];
static unsigned startxref;

static unsigned pdf_root_obj = 0, pdf_info_obj = 0;

/* Internal static routines */

static void pdf_flush_obj (FILE *file, const pdf_obj *object);
static void pdf_label_obj (pdf_obj *object);

static void pdf_out_char (FILE *file, char c);
static void pdf_out (FILE *file, char *buffer, int length);

static void release_indirect (struct pdf_indirect *data);
static void write_indirect (FILE *file, const struct pdf_indirect *indirect);

static void release_boolean (struct pdf_obj *data);
static void write_boolean (FILE *file, const struct pdf_boolean *data);

static void write_null (FILE *file, void *data);
static void release_null (void *data);

static void release_number (struct pdf_number *data);
static void write_number (FILE *file, const struct pdf_number
			  *number);

static void write_string (FILE *file, const struct pdf_string *string);
static void release_string (struct pdf_string *data);

static void write_name (FILE *file, const struct pdf_name *name);
static void release_name (struct pdf_name *data);

static void write_array (FILE *file, const struct pdf_array *array);
static void release_array (struct pdf_array *data);

static void write_dict (FILE *file, const struct pdf_dict *dict);
static void release_dict (struct pdf_dict *data);

static void write_stream (FILE *file, const struct pdf_stream *stream);
static pdf_obj *release_stream (struct pdf_stream *stream);

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
  if (!(pdf_file = fopen (filename, "w"))) {
    if (strlen(filename) < 128) {
      sprintf (format_buffer, "Unable to open %s\n", filename);
    } else
      sprintf (format_buffer, "Unable to open file");
    ERROR (format_buffer);
  }
  pdf_out (pdf_file, "%PDF-1.2\n", 9);
}

static void dump_xref(void)
{
  int length, i;
  startxref = file_position;	/* Record where this xref is for
				   trailer */
  pdf_out (pdf_file, "xref\n", 5);
  length = sprintf (format_buffer, "%d %d\n", 0, next_label);
  pdf_out (pdf_file, format_buffer, length);
  length = sprintf (format_buffer, "%010ld %05ld f \n", 0L, 65535L);
  /* Every space counts.  The space after the 'f' and 'n' is
   *essential*.  The PDF spec says the lines must be 20 characters
   long including the end of line character. */
  pdf_out (pdf_file, format_buffer, length);
  for (i=1; i<next_label; i++){
    length = sprintf (format_buffer, "%010ld %05ld n \n",
		      xref[i-1].file_position, 0L);
    pdf_out (pdf_file, format_buffer, length);
  }
}

static void dump_trailer(void)
{
  int length;
  unsigned long starttrailer;
  starttrailer = file_position;
  pdf_out (pdf_file, "trailer\n", 8);
  pdf_out (pdf_file, "<<\n", 3);
  length = sprintf (format_buffer, "/Size %ld\n",
		    next_label);
  pdf_out (pdf_file, format_buffer, length);
  if (pdf_root_obj == 0) 
    ERROR ("dump_trailer:  Invalid root object");
  length = sprintf (format_buffer, "/Root %u %u R\n", pdf_root_obj, 0);
  pdf_out (pdf_file, format_buffer, length);
  if (pdf_info_obj != 0) {
    length = sprintf (format_buffer, "/Info %u %u R\n", pdf_info_obj, 0);
    pdf_out (pdf_file, format_buffer, length);
  }
  pdf_out (pdf_file, ">>\n", 3);
  pdf_out (pdf_file, "startxref\n", 10);
  length = sprintf (format_buffer, "%ld\n", startxref);
  pdf_out (pdf_file, format_buffer, length);
  pdf_out (pdf_file, "%%EOF\n", 6);
}


void pdf_out_flush (void)
{
  if (pdf_file) {
    if (verbose) fprintf (stderr, "pdf_obj_out_flush:  dumping xref\n");
    dump_xref();
    if (verbose) fprintf (stderr, "pdf_obj_out_flush:  dumping trailer\n");
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

static void pdf_out_char (FILE *file, char c)
{
  fputc (c, file);
  /* Keep tallys for xref table *only* if writing a pdf file */
  if (file == pdf_file)
    file_position += 1;
}

static void pdf_out (FILE *file, char *buffer, int length)
{
  fwrite (buffer, 1, length, file);
  /* Keep tallys for xref table *only* if writing a pdf file */
  if (file == pdf_file)
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
  if (object == NULL)
    return;
  if (next_label > PDF_MAX_IND_OBJECTS) {
    ERROR ("pdf_label_obj:  Indirect object capacity exceeded");
  }
  if (object -> label == 0) {  /* Don't change label on an already labeled
				  object.  Ignore such calls */
    /* Save so we can lookup this object by its number */
    xref[next_label-1].pdf_obj = object;
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

pdf_obj *pdf_new_ref (int label, int generation) 
{
  pdf_obj *result;
  struct pdf_indirect *indirect;
  result = pdf_new_obj (PDF_INDIRECT);
  indirect = NEW (1, struct pdf_indirect);
  result -> data = indirect;
  indirect -> label = label;
  indirect -> generation = generation;
  indirect -> dirty = 1;  /* Any caller that directly sets his own label
			     is dirty */
  return result;
}

pdf_obj *pdf_ref_obj(pdf_obj *object)
{
  pdf_obj *result;
  struct pdf_indirect *indirect;
  
  if (object == NULL)
    ERROR ("pdf_ref_obj passed null pointer");
  
  if (object -> type == 0) {
    ERROR ("pdf_ref_obj:  Called with invalid object");
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
    indirect -> dirty = ((struct pdf_indirect *) (object -> data)) -> dirty;
  } else {
    if (object -> label == 0) {
      pdf_label_obj (object);
    }
    indirect -> label = object -> label;
    indirect -> generation = object -> generation;
    indirect -> dirty = 0;
  }
  return result;
}

static void release_indirect (struct pdf_indirect *data)
{
  free (data);
}

static void write_indirect (FILE *file, const struct pdf_indirect *indirect)
{
  int length;
  if (indirect -> dirty) {
    if (file == stderr) 
      pdf_out (file, "{d}", 3);
    else
      fprintf (stderr, "\nTried to write a dirty object\n");
    pdf_out (file, "        0       0    R", 22);
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

static void write_null (FILE *file, void *data)
{
  pdf_out (file, "null", 4);
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

static void write_boolean (FILE *file, const struct pdf_boolean *data)
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

static void write_number (FILE *file, const struct pdf_number *number)
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
   ((struct pdf_number *) (object -> data)) -> value = value;
}

double pdf_number_value (pdf_obj *object)
{
  if (object == NULL || object -> type != PDF_NUMBER) {
    ERROR ("pdf_obj_number_value:  Passed non-number object");
  }
  return ((struct pdf_number *)(object -> data)) -> value;
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


static void write_string (FILE *file, const struct pdf_string *string)
{
  char *s = string -> string;
  int i, count;
  pdf_out_char (file, '(');
  for (i=0; i< string -> length; i++) {
    count = pdfobj_escape_c (format_buffer, s[i]);
    pdf_out (file, format_buffer, count);
  }
  pdf_out_char (file, ')');
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

int pdf_check_name(const char *name)
{
  static char *valid_chars =
    "!\"&'*+,-.0123456789:;=?@ABCDEFGHIJKLMNOPQRSTUVWXYZ\\^_`abcdefghijklmnopqrstuvwxyz|~";
  int i;
  if (strspn (name, valid_chars) == strlen (name))
    return 1;
  else
    return 0;
}

pdf_obj *pdf_new_name (const char *name)  /* name does *not* include the / */ 
{
  pdf_obj *result;
  unsigned length = strlen (name);
  struct pdf_name *data;
  if (!pdf_check_name (name)) {
    fprintf (stderr, "%s\n", name);
    ERROR ("pdf_new_name:  invalid PDF name");
  }
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

int pdf_match_name (const pdf_obj *name_obj, const char *name_string)
{
  struct pdf_name *data;
  data = name_obj -> data;
  return (!strcmp (data -> name, name_string));
}


static void write_name (FILE *file, const struct pdf_name *name)
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

char *pdf_name_value (pdf_obj *object)
{
  struct pdf_name *data;
  char *result;
  if (object == NULL || object -> type != PDF_NAME) {
     ERROR ("pdf_name_value:  Passed non-name object");
  }
  data = object -> data;
  if (data -> name == NULL)
    return NULL;
  result = NEW (strlen (data -> name)+1, char);
  strcpy (result, data -> name);
  return result;
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

static void write_array (FILE *file, const struct pdf_array *array)
{
  pdf_out_char (file, '[');
  while (array -> next != NULL) {
    pdf_write_obj (file, array -> this);
    array = array -> next;
    if (array -> next != NULL)
      pdf_out_char (file, ' ');
  }
  pdf_out_char (file, ']');
}

pdf_obj *pdf_get_array (pdf_obj *array, int index)
{
  struct pdf_array *data;
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
  return pdf_link_obj (data -> this);
}



static void release_array (struct pdf_array *data)
{
  struct pdf_array *next;
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
  struct pdf_array *data;
  struct pdf_array *new_node;
  if (array == NULL || array -> type != PDF_ARRAY) {
     ERROR ("pdf_add_array:  Passed non-array object");
  }
  data = array -> data;
  new_node = NEW (1, struct pdf_array);
  new_node -> this = NULL;
  new_node -> next = NULL;
  while (data -> next != NULL)
    data = data -> next;
  data -> next = new_node;
  data -> this = pdf_link_obj (object);
}


static void write_dict (FILE *file, const struct pdf_dict *dict)
{
  pdf_out (file, "<<\n", 3);
  while (dict -> key != NULL) {
    pdf_write_obj (file, dict -> key);
    pdf_out_char (file, ' ');
    pdf_write_obj (file, dict -> value);
    dict = dict -> next;
    pdf_out_char (file, '\n');
  }
  pdf_out (file, ">>", 2);
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
  struct pdf_dict *data;
  struct pdf_dict *new_node;
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
  new_node = NEW (1, struct pdf_dict);
  new_node -> key = NULL;
  new_node -> value = NULL;
  new_node -> next = NULL;
  while (data -> key != NULL)
    data = data -> next;
  data -> next = new_node;
  data -> key = pdf_link_obj (key);
  data -> value = pdf_link_obj (value);
}

void pdf_merge_dict (pdf_obj *dict1, pdf_obj *dict2)
{
  struct pdf_dict *data;
  if (dict1 == NULL || dict1 -> type != PDF_DICT) 
    ERROR ("pdf_merge_dict:  Passed invalid first dictionary");
  if (dict2 == NULL || dict2 -> type != PDF_DICT)
    ERROR ("pdf_merge_dict:  Passed invalid second dictionary");
  data = dict2 -> data;
  while (data -> key != NULL) {
    struct pdf_name *key = (data -> key) -> data;
    if (key == NULL || pdf_lookup_dict (dict1, key -> name) == NULL) {
      pdf_add_dict (dict1, data -> key, data -> value);
    }
    data = data -> next;
  }
}

pdf_obj *pdf_lookup_dict (const pdf_obj *dict, const char *name)
{
  struct pdf_dict *data;
  if (dict == NULL || dict ->type != PDF_DICT) 
    ERROR ("pdf_lookup_dict:  Passed invalid dictionary");
  data = dict -> data;
  while (data -> key != NULL) {
    if (pdf_match_name (data -> key, name))
      return (pdf_link_obj (data -> value));
    data = data -> next;
  }
  return NULL;
}

char *pdf_get_dict (const pdf_obj *dict, int index)
{
  struct pdf_dict *data;
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

static void write_stream (FILE *file, const struct pdf_stream *stream)
{
  int ch, length = 0, nwritten = 0;
  pdf_write_obj (file, stream -> dict);
  pdf_out (file, "\nstream\n", 8);
  rewind (stream -> tmpfile);
  while ((nwritten = fread (work_buffer, sizeof (char),
			    WORK_BUFFER_SIZE,
			    stream -> tmpfile)) > 0) {
    pdf_out (file, work_buffer, nwritten);
    length += nwritten;
  }
  pdf_out (file, "\n", 1);
  pdf_set_number (stream -> length, length+1);
  pdf_out (file, "endstream", 9);
}


static pdf_obj *release_stream (struct pdf_stream *stream)
{
  pdf_release_obj (stream -> dict);
  pdf_release_obj (stream -> length);
  fclose (stream -> tmpfile);
}

pdf_obj *pdf_stream_dict (pdf_obj *stream)
{
  struct pdf_stream *data;
  if (stream == NULL || stream -> type != PDF_STREAM) {
     ERROR ("pdf_stream_dict:  Passed non-stream object");
  }
  data = stream -> data;
  return pdf_link_obj (data -> dict);
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

void pdf_write_obj (FILE *file, const pdf_obj *object)
{
  int length;
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
    write_null (file, object -> data);
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
  xref[object->label-1].file_position = file_position;
  length = sprintf (format_buffer, "%d %d obj\n", object -> label ,
		    object -> generation);
  pdf_out (file, format_buffer, length);
  pdf_write_obj (file, object);
  pdf_out (file, "\nendobj\n", 8);
}


void pdf_release_obj (pdf_obj *object)
{
  int length;
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
    if (object -> label && pdf_file != NULL) { 
      pdf_flush_obj (pdf_file, object);
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
    }
    object -> type = -1;  /* This might help detect freeing already freed objects */
    free (object);
  }
}

