#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "pdfspecial.h"
#include "pdfobj.h"
#include "pdfdoc.h"
#include "pdfdev.h"
#include "numbers.h"
#include "mem.h"
#include "dvi.h"
#include "io.h"

#define verbose 0

static pdf_obj *parse_pdf_object (char **start, char *end);
static pdf_obj *parse_pdf_dict (char **start, char *end);
static pdf_obj *parse_pdf_array (char **start, char *end);
static pdf_obj *parse_pdf_string (char **start, char *end);
static pdf_obj *parse_pdf_name (char **start, char *end);
static char *parse_ident (char **start, char *end);
static char *parse_number (char **start, char *end);
static pdf_obj *parse_pdf_number (char **start, char *end);
static pdf_obj *parse_pdf_boolean (char **start, char *end);
static char *parse_pdf_reference (char **start, char *end);
static pdf_obj *get_pdf_reference (char **start, char *end);
static void parse_crap (char **start, char *end);

static void skip_white (char **start, char *end);
static void skip_line (char **start, char *end);

static void add_reference (char *name, pdf_obj *object);
static void release_reference (char *name);
static pdf_obj *lookup_reference(char *name);

static pdf_obj *pdf_new_ref (int label, int generation);

static int dump(char *start, char *end)
{
  char *p = start;
  fprintf (stderr, "\nCurrent input buffer is ");
  fprintf (stderr, "-->");
  while (p < end)
    fprintf (stderr, "%c", *(p++));
  fprintf (stderr, "<--\n");
}

static void skip_white (char **start, char *end)
{
  while (*start < end && isspace (**start))
    (*start)++;
  return;
}

static void skip_line (char **start, char *end)
{
  /* PDF spec says that all platforms must end line with '\n' here */
  while (*start < end && **start != '\n') 
    (*start)++;
  return;
}


static void parse_crap (char **start, char *end)
{
  skip_white(start, end);
  if (*start != end) {
    fprintf (stderr, "\nCrap left over after object!!\n");
    dump(*start, end);
  }
}

static void do_bop(char **start, char *end)
{
  if (*start != end)
    pdf_doc_bop (*start, end - *start);
  return;
}

static void do_eop(char **start, char *end)
{
  if (*start != end)
    pdf_doc_eop (*start, end - *start);
}


static void do_put(char **start, char *end)
{
  pdf_obj *result, *data;

  skip_white(start, end);
  if ((result = get_pdf_reference(start, end)) == NULL) {
    fprintf (stderr, "\nPUT:  Nonexistent object reference\n");
    return;
  }
  
  if (result -> type == PDF_DICT) {
    if ((data = parse_pdf_dict (start, end)) == NULL) {
      pdf_release_obj (result);
      return;
    }
    pdf_merge_dict (result, data);
    return;
  }
  if (result -> type == PDF_ARRAY) {
    if ((data = parse_pdf_array (start, end)) == NULL) {
      pdf_release_obj (result);
      return;
    }
    pdf_add_array (result, data);
    return;
  }
  else {
    fprintf (stderr, "\nPUT:  Invalid object type\n");
    return;
  }
}

static void do_epdf (char **start, char *end, double x_user, double y_user)
{
  char *name;
  skip_white(start, end);
  dump(*start, end);
  if (*start < end && (name = parse_ident(start, end)) != NULL) {
    fprintf (stderr, "Opening %s\n", name);
    pdf_open (name, x_user, y_user);
    release (name);
  } else
    {
      fprintf (stderr, "No file name found\n");
      dump(*start, end);
    }
}


static int is_a_number(const char *s)
{
  int i;
  for (i=0; i<strlen(s); i++) {
    if (!isdigit (*s))
      return 0;
  }
  return 1;
}

static char *parse_opt_ident(char **start, char *end)
{
  skip_white(start, end);
  if (*start  >= end || (**start) != '@')
    return NULL;
  (*start)++;
  return parse_ident(start, end);
}

#define WIDTH 1
#define HEIGHT 2
#define DEPTH 3

struct {
  char *s;
  int dimension;
} dimensions[] = {
  {"width", WIDTH},
  {"height", HEIGHT},
  {"depth", DEPTH}
};

static int parse_dimension (char **start, char *end)
{
  int i;
  char *dimension_string;
  skip_white(start, end);
  if ((dimension_string = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "\nExpecting a dimension here\n");
    dump(*start, end);
  }
  for (i=0; i<3; i++) {
    if (!strcmp (dimensions[i].s, dimension_string))
      break;
  }
  if (i == 3) {
    release (dimension_string);
    fprintf (stderr, "%s: Invalid dimension\n", dimension_string);
    return -1;
  }
  release (dimension_string);
  return dimensions[i].dimension;
}

struct {
  char *s;
  double units;
} units[] = {
  {"pt", (72.0/72.27)},
  {"in", (72.0)},
  {"cm", (72.0/2.54)}
};
  
static double parse_units (char **start, char *end)
{
  int i;
  char *unit_string;
  skip_white(start, end);
  if ((unit_string = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "\nExpecting a unit here\n");
    dump(*start, end);
  }
  for (i=0; i<3; i++) {
    if (!strcmp (units[i].s, unit_string))
      break;
  }
  if (i == 3) {
    fprintf (stderr, "%s: Invalid dimension\n", unit_string);
    release (unit_string);
    return -1.0;
  }
  release (unit_string);
  return units[i].units*dvi_tell_mag();
}

static void do_ann(char **start, char *end)
{
  pdf_obj *result, *rectangle, *tmp1, *tmp2;
  char *name, *number_string;
  int dimension;
  double value, units;
  double height=0.0, width=0.0, depth=0.0;
  skip_white(start, end);
  name = parse_opt_ident(start, end);
  skip_white(start, end);
  while ((*start) < end && isalpha (**start)) {
    skip_white(start, end);
    if ((dimension = parse_dimension(start, end)) < 0 ||
	(number_string = parse_number(start, end)) == NULL ||
	(units = parse_units(start, end)) < 0.0) {
      fprintf (stderr, "\nExpecting dimensions for annotation\n");
      dump (*start, end);
      return;
    }
    switch (dimension) {
    case WIDTH:
      width = atof (number_string)*units;
      break;
    case HEIGHT:
      height = atof (number_string)*units;
      break;
    case DEPTH:
      depth = atof (number_string)*units;
      break;
    }
    release(number_string);
    skip_white(start, end);
  }
  if (width == 0.0 || depth + height == 0.0) {
    fprintf (stderr, "ANN: Rectangle has a zero dimension\n");
    return;
  }

  if ((result = parse_pdf_dict(start, end)) == NULL) {
    fprintf (stderr, "Ignoring invalid dictionary\n");
    return;
  };
  rectangle = pdf_new_array();
  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_x(),0.1)));
  pdf_release_obj (tmp1);

  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_y()-depth,0.1)));
  
  pdf_release_obj (tmp1);

  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_x()+width,0.1)));
  pdf_release_obj (tmp1);

  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_y()+height,0.1)));
  pdf_release_obj (tmp1);

  pdf_add_dict (result, tmp1 = pdf_new_name ("Rect"),
		rectangle);
  pdf_release_obj (tmp1);
  pdf_release_obj (rectangle);

  pdf_doc_add_to_page_annots (tmp1 = pdf_ref_obj (result));
  pdf_release_obj (tmp1);

  if (name != NULL) {
    add_reference (name, result);
    /* An annotation is treated differently from a cos object.
       The previous line adds both a direct link and an indirect link
       to "result".  For cos objects this prevents the direct link
       prevents the object from being flushed immediately.  
       So that an ANN doesn't behave like an OBJ, the ANN should be
       immediately unlinked.  Otherwise the ANN would have to be
       closed later. This seems awkward, but an annotation is always
       considered to be complete */
    release_reference (name);
    release (name);
  }

  pdf_release_obj (result);

  parse_crap(start, end);
  return;
}


static void do_outline(char **start, char *end)
{
  pdf_obj *result;
  char *level;
  skip_white(start, end);

  if ((level = parse_ident(start, end)) == NULL ||
      !is_a_number (level)) {
    fprintf (stderr, "\nExpecting number for object level\n");
    dump (*start, end);
    return;
  }
  if ((result = parse_pdf_dict(start, end)) == NULL) {
    fprintf (stderr, "Ignoring invalid dictionary\n");
    return;
  };
  parse_crap(start, end);

  pdf_doc_change_outline_depth (atoi (level));
  release (level);
  pdf_doc_add_outline (result);
  pdf_release_obj (result);
  return;
}

static void do_article(char **start, char *end)
{
  char *name, *save = *start;
  pdf_obj *info_dict, *article_dict;
  skip_white (start, end);
  if (*((*start)++) != '@' || (name = parse_ident(start, end)) == NULL) {
    fprintf (stderr, "Article name expected.\n");
    *start = save;
    dump(*start, end);
  }
  if ((info_dict = parse_pdf_dict(start, end)) == NULL) {
    release (name);
    fprintf (stderr, "Ignoring invalid dictionary\n");  
  }
  add_reference (name, article_dict = pdf_doc_add_article (name, info_dict));
  release (name);
  pdf_release_obj (info_dict);
  pdf_release_obj (article_dict);
  parse_crap(start, end);
}

static void do_bead(char **start, char *end)
{
  pdf_obj *bead_dict, *rectangle, *tmp1, *tmp2;
  char *name, *number_string, *save = *start;
  int dimension;
  double value, units;
  double height=0.0, width=0.0, depth=0.0;
  skip_white(start, end);
  name = parse_opt_ident(start, end);
  skip_white(start, end);
  while (*start < end && isalpha (**start)) {
    skip_white(start, end);
    if ((dimension = parse_dimension(start, end)) < 0 ||
	(number_string = parse_number(start, end)) == NULL ||
	(units = parse_units(start, end)) < 0.0) {
      fprintf (stderr, "\nExpecting dimensions for bead\n");
      dump (*start, end);
      return;
    }
    switch (dimension) {
    case WIDTH:
      width = atof (number_string)*units;
      break;
    case HEIGHT:
      height = atof (number_string)*units;
      break;
    case DEPTH:
      depth = atof (number_string)*units;
      break;
    }
    release(number_string);
    skip_white(start, end);
  }
  if (width == 0.0 || depth + height == 0.0) {
    fprintf (stderr, "BEAD: Rectangle has a zero dimension\n");
    return;
  }

  bead_dict = pdf_new_dict ();
  rectangle = pdf_new_array();
  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_x(),0.1)));
  pdf_release_obj (tmp1);

  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_y()-depth,0.1)));
  
  pdf_release_obj (tmp1);

  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_x()+width,0.1)));
  pdf_release_obj (tmp1);

  pdf_add_array (rectangle, tmp1 = pdf_new_number(ROUND(dev_tell_y()+height,0.1)));
  pdf_release_obj (tmp1);

  pdf_add_dict (bead_dict, tmp1 = pdf_new_name ("R"),
		rectangle);
  pdf_release_obj (tmp1);
  pdf_release_obj (rectangle);

  pdf_add_dict (bead_dict, tmp1 = pdf_new_name ("P"),
		tmp2 = pdf_doc_this_page());
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);

  pdf_doc_add_bead (name, bead_dict);

  if (name != NULL) {
    release (name);
  }
  pdf_release_obj (bead_dict);
  parse_crap(start, end);
  return;
}

static void do_dest(char **start, char *end)
{
  pdf_obj *name;
  pdf_obj *array;
  skip_white(start, end);
  if ((name = parse_pdf_string(start, end)) == NULL) {
    fprintf (stderr, "\nPDF string expected and not found.\n");
    fprintf (stderr, "DEST Special ignored\n");
    dump(*start, end);
    return;
  }
  if ((array = parse_pdf_array(start, end)) == NULL)
    return;

  pdf_doc_add_dest (pdf_obj_string_value(name), pdf_obj_string_length(name), array);
  pdf_release_obj (name);
  pdf_release_obj (array);
}

static void do_docinfo(char **start, char *end)
{
  pdf_obj *result = parse_pdf_dict(start, end);
  pdf_doc_merge_with_docinfo (result);
  parse_crap(start, end);
  return;
}

static void do_docview(char **start, char *end)
{
  pdf_obj *result = parse_pdf_dict(start, end);
  pdf_doc_merge_with_catalog (result);
  parse_crap(start, end);
  return;
}


static void do_close(char **start, char *end)
{
  char *name;
  skip_white(start, end);
  if ((name = parse_pdf_reference(start, end)) != NULL) {
    release_reference (name);
    release (name);
  }
  parse_crap(start, end);
  return;
}

static void do_obj(char **start, char *end)
{
  pdf_obj *result;
  char *name;
  skip_white(start, end);
  name = parse_opt_ident(start, end);

  if ((result = parse_pdf_object(start, end)) == NULL) {
    fprintf (stderr, "Special Ignored.\n");
    return;
  };

  parse_crap(start, end);
  
  if (name != NULL) {
    add_reference (name, result);
    release (name);
  }
  pdf_release_obj (result);  /* Object won't be shipped until CLOSE is
				called */
  return;
}


void do_content(char **start, char *end)
{
  skip_white(start, end);
  pdf_doc_add_to_page (*start, end-*start);
}


static int is_pdf_special (char **start, char *end)
{
  skip_white(start, end);
  if (end-*start >= strlen ("pdf:") &&
      !strncmp (*start, "pdf:", strlen("pdf:"))) {
    *start += strlen("pdf:");
    return 1;
  }
  return 0;
}

#define ANN 1
#define OUT 2
#define ARTICLE 3
#define DEST 4
#define DOCINFO 7
#define DOCVIEW 8
#define OBJ 9
#define CONTENT 10
#define PUT 11
#define CLOSE 12
#define BOP 13
#define EOP 14
#define BEAD 15
#define EPDF 16

struct pdfmark
{
  char *string;
  int value;
} pdfmarks[] = {
  {"ANN", ANN},
  {"ANNOT", ANN},
  {"ANNOTATE", ANN},
  {"ANNOTATION", ANN},
  {"OUT", OUT},
  {"OUTLINE", OUT},
  {"ART", ARTICLE},
  {"ARTICLE", ARTICLE},
  {"BEAD", BEAD},
  {"DEST", DEST},
  {"DOCINFO", DOCINFO},
  {"DOCVIEW", DOCVIEW},
  {"OBJ", OBJ},
  {"OBJECT", OBJ},
  {"CONTENT", CONTENT},
  {"PUT", PUT},
  {"CLOSE", CLOSE},
  {"BOP", BOP},
  {"EOP", EOP},
  {"EPDF", EPDF}
};


static int parse_pdfmark (char **start, char *end)
{
  char *save;
  int i;
  if (verbose) {
    fprintf (stderr, "\nparse_pdfmark:");
    dump (*start, end);
  }
  skip_white(start, end);
  if (*start >= end) {
    fprintf (stderr, "Special ignored...no pdfmark found\n");
    return -1;
  }
  
  save = *start;
  while (*start < end && isalpha (**start))
    (*start)++;
  for (i=0; i<sizeof(pdfmarks)/sizeof(struct pdfmark); i++) {
    if (*start-save == strlen (pdfmarks[i].string) &&
	!strncmp (save, pdfmarks[i].string,
		  strlen(pdfmarks[i].string)))
      return pdfmarks[i].value;
  }
  *start = save;
  fprintf (stderr, "\nExpecting pdfmark (and didn't find one)\n");
  dump(*start, end);
  return -1;
}

static pdf_obj *parse_pdf_dict (char **start, char *end)
{
  pdf_obj *result, *tmp1, *tmp2;
  skip_white(start, end);
  if (*((*start)++) != '<' ||
      *((*start)++) != '<')
    return NULL;
  result = pdf_new_dict ();
    skip_white(start, end);
  while (*start < end &&
	 **start != '>') {
    if ((tmp1 = parse_pdf_name (start, end)) == NULL) {
      pdf_release_obj (result);
      return NULL;
    };
    if ((tmp2 = parse_pdf_object (start, end)) == NULL) {
      pdf_release_obj (result);
      pdf_release_obj (tmp1);
      return NULL;
    }
    pdf_add_dict (result, tmp1, tmp2);
    pdf_release_obj (tmp1);
    pdf_release_obj (tmp2);
    skip_white(start, end);
  }
  if (*start >= end) {
    pdf_release_obj (result);
    return NULL;
  }
  if (*((*start)++) == '>' &&
      *((*start)++) == '>') {
    return result;
  } else {
    pdf_release_obj (result);
    fprintf (stderr, "\nDictionary object ended prematurely\n");
    return NULL;
  }
}

static pdf_obj *parse_pdf_array (char **start, char *end)
{
  pdf_obj *result, *tmp1;
  skip_white(start, end);
  if (*((*start)++) != '[')
    return NULL;
  result = pdf_new_array ();
  skip_white(start, end);
  while (*start < end &&
	 **start != ']') {
    if ((tmp1 = parse_pdf_object (start, end)) == NULL) {
      pdf_release_obj (result);
      return NULL;
    };
    pdf_add_array (result, tmp1);
    pdf_release_obj (tmp1);
    skip_white(start, end);
  }
  if (*start >= end) {
    pdf_release_obj (result);
    fprintf (stderr, "\nArray ended prematurely\n");
    return NULL;
  }
  (*start)++;
  return result;
}

static char *parse_number (char **start, char *end)
{
  char *number, *save;
  int length;
  skip_white(start, end);
  save = *start;
  while (*start < end &&
	 isdigit(**start))
    (*start)++;
  if (*start < end && **start == '.') {
    (*start)++;
    while (*start < end &&
	   isdigit(**start))
      (*start)++;
  }
  if (*start > save) {
    number = NEW ((*start-save)+1, char);
    strncpy (number, save, (*start-save));
    number[*start-save] = 0;
    return number;
  }
  *start = save;
  return NULL;
}


static char *parse_ident (char **start, char *end)
{
  char *ident, *save;
  static char *valid_chars =
    "!\"&'*+,-.0123456789:;=?@ABCDEFGHIJKLMNOPQRSTUVWXYZ\\^_`abcdefghijklmnopqrstuvwxyz|~";
  skip_white(start, end);
  save = *start;
  while (*start < end && strchr (valid_chars, **start))
    (*start)++;
  if (save == *start)
    return NULL;
  ident = NEW (*start-save+1, char);
  strncpy (ident, save, *start-save);
  ident[*start-save] = 0;
  return ident;
}

static pdf_obj *parse_pdf_name (char **start, char *end)
{
  pdf_obj *result;
  char *name;
  skip_white(start, end);
  if (**start != '/') {
    fprintf (stderr, "\nPDF Name expected and not found.\n");
    dump(*start, end);
    return NULL;
  }
  (*start)++;
  if ((name = parse_ident(start, end)) != NULL) {
    result = pdf_new_name (name);
    release (name);
    return result;
  }
  return NULL;
}

#define MAX_NAMED_REFERENCES 256
struct named_reference 
{
  char *name;
  pdf_obj *object_ref;
  pdf_obj *object;
} named_references[MAX_NAMED_REFERENCES];
static unsigned number_named_references = 0;


static void add_reference (char *name, pdf_obj *object)
{
  int i;
  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i != number_named_references) {
    fprintf (stderr, "\nWarning: @%s: Duplicate named reference ignored\n", name);
  }
  named_references[number_named_references].name = NEW (strlen
							(name)+1,
							char);
  strcpy (named_references[number_named_references].name, name);
  named_references[number_named_references].object_ref = pdf_ref_obj(object);
  named_references[number_named_references].object = pdf_link_obj (object);
  number_named_references+=1;
}

static pdf_obj *lookup_reference(char *name)
{
  int i;
  /* First check for builtin */
  if (!strcmp (name, "thispage")) {
    return pdf_doc_this_page();
  }
  if (!strcmp (name, "ypos")) {
    return pdf_new_number(ROUND(dev_tell_y(),0.1));
  }
  if (!strcmp (name, "xpos")) {
    return pdf_new_number(ROUND(dev_tell_x(),0.1));
  }
  if (strlen (name) > 4 &&
      !strncmp (name, "page", 4) &&
      is_a_number (name+4)) {
    return pdf_doc_ref_page(atoi (name+4));
  }
  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i == number_named_references)
    return NULL;
  return (pdf_link_obj (named_references[i].object_ref));
}

static pdf_obj *lookup_object(char *name)
{
  int i;
  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i == number_named_references)
    return NULL;
  if (named_references[i].object == NULL)
    fprintf (stderr, "lookup_object: Referenced object not defined\n");
  return (pdf_link_obj (named_references[i].object));
}

static void release_reference (char *name)
{
  int i;
  for (i=0; i<number_named_references; i++) {
    if (!strcmp (named_references[i].name, name)) {
      break;
    }
  }
  if (i == number_named_references) {
    fprintf (stderr, "release_reference: tried to release nonexistent reference\n");
    return;
  }
  pdf_release_obj (named_references[i].object);
  named_references[i].object = NULL;
}


static char *parse_pdf_reference(char **start, char *end)
{
  char *name;
  skip_white(start, end);
  if (**start != '@') {
    fprintf (stderr, "\nPDF Name expected and not found.\n");
    dump(*start, end);
    return NULL;
  }
  (*start)++;
  return parse_ident(start, end);
}

static pdf_obj *get_pdf_reference(char **start, char *end)
{
  char *name, *save = *start;
  pdf_obj *result;
  if ((name = parse_pdf_reference(start, end)) != NULL) {
    result = lookup_reference (name);
    if (result == NULL) {
      fprintf (stderr, "\nNamed reference (@%s) doesn't exist.\n", name);
      *start = save;
      dump(*start, end);
    }
    release (name);
    return result;
  }
  return NULL;
}

void pdf_finish_specials (void)
{
  int i;
  for (i=0; i<number_named_references; i++) {
    pdf_release_obj (named_references[i].object_ref);
    if (named_references[i].object != NULL) {
      fprintf(stderr, "\nWarning: Named Object %s never closed\n", named_references[i].name);
      pdf_release_obj (named_references[i].object);
    }
    release (named_references[i].name);
  }
}

static pdf_obj *parse_pdf_boolean (char **start, char *end)
{
  skip_white(start, end);
  if (end-*start > strlen ("true") &&
      !strncmp (*start, "true", strlen("true"))) {
    *start += strlen("true");
    return pdf_new_boolean (1);
  }
  if (end - *start > strlen ("false") &&
      !strncmp (*start, "false", strlen("false"))) {
    *start += strlen("false");
    return pdf_new_boolean (0);
  }
  return NULL;
}

static pdf_obj *parse_pdf_null (char **start, char *end)
{
  char *save = *start;
  char *ident;
  pdf_obj *result;
  skip_white(start, end);
  ident = parse_ident(start, end);
  if (!strcmp (ident, "null")) {
    release(ident);
    return pdf_new_null();
  }
  *start = save;
  fprintf (stderr, "\nNot a valid object\n");
  dump(*start, end);
}

static pdf_obj *parse_pdf_number (char **start, char *end)
{
  char *number;
  pdf_obj *result;
  skip_white(start, end);
  if ((number = parse_number(start, end)) != NULL) {
    result = pdf_new_number (atof(number));
    release (number);
    return result;
  }
  return NULL;
}
  
static pdf_obj *parse_pdf_string (char **start, char *end)
{
  pdf_obj *result;
  char *save, *string;
  int strlength;
  skip_white(start, end);
  if (*((*start)++) != '(')
    return NULL;
  save = *start;
  string = NEW (end - *start, char);
  strlength = 0;
  while (*start < end &&
	 **start != ')') {
    if (**start == '\\')
      switch (*(++(*start))) {
      case 'n':
	string[strlength++] = '\n';
	(*start)++;
	break;
      case 'r':
	string[strlength++] = '\r';
	(*start)++;
	break;
      case 't':
	string[strlength++] = '\t';
	(*start)++;
	break;
      case 'b':
	string[strlength++] = '\b';
	(*start)++;
	break;
      default:
	if (isdigit(**start)) {
	  int i;
	  string[strlength] = 0;
	  for (i=0; i<3; i++) 
	    string[strlength] = string[strlength]*8 + (*((*start)++)-'0');
	}
      }
    else
      string[strlength++] = *((*start)++);
  }
  if (*start >= end) {
    fprintf (stderr, "\nString object ended prematurely\n");
    return NULL;
  }
  (*start)++;
  result = pdf_new_string (string, strlength);
  release (string);
  return result;
}

static pdf_obj *parse_pdf_stream (char **start, char *end, pdf_obj
				  *dict)
{
  pdf_obj *result, *new_dict, *tmp1, *tmp2;
  int length;
  if (pdf_lookup_dict(dict, "Filter") ||
      pdf_lookup_dict(dict, "F")) {
    fprintf (stderr, "Cannot handle filtered streams or file streams yet");
    return NULL;
  }
  if ((tmp1 = pdf_lookup_dict(dict, "Length")) == NULL) {
    fprintf (stderr, "No length specified");
    return NULL;
  }
  pdf_write_obj (stderr, tmp1);
  tmp2 = pdf_deref_obj (tmp1);
  pdf_release_obj (tmp1);
  length = pdf_number_value (tmp2);
  pdf_release_obj (tmp2);
  fprintf (stderr, "stream length is %d\n", length);
  skip_white(start, end);
  skip_line(start, end);
  result = pdf_new_stream();
  new_dict = pdf_stream_dict(result);
  pdf_merge_dict (new_dict, dict);
  pdf_release_obj (new_dict);
  pdf_add_stream (result, *start, length);
  *start += length;
  skip_white(start, end);
  fprintf (stderr, "End of stream?[%s]\n", *start);
  if (*start+strlen("endstream") > end ||
      strncmp(*start, "endstream", strlen("endstream"))) {
    fprintf (stderr, "\nendstream not found\n");
    return NULL;
  }
  *start += strlen("endstream");
  return result;
}



static pdf_obj *parse_pdf_object (char **start, char *end)
{
  pdf_obj *result, *tmp1=NULL, *tmp2=NULL;
  char *save = *start;
  char *position2;
  skip_white(start, end);
  if (*start < end) switch (**start) {
  case '<': 
    result = parse_pdf_dict (start, end);
    skip_white(start, end);
    if (end - *start > strlen("stream") &&
	!strncmp(*start, "stream", strlen("stream"))) {
      result = parse_pdf_stream (start, end, result);
    }
    /* Check for stream */
    break;
  case '(':
    result = parse_pdf_string(start, end);
    break;
  case '[':
    result = parse_pdf_array(start, end);
    break;
  case '/':
    result = parse_pdf_name(start, end);
    break;
  case '@':
    result = get_pdf_reference(start, end);
    break;
  case 't':
  case 'f':
    result = parse_pdf_boolean(start, end);
    break;
  default:
    /* This is a bit of a hack, but PDF doesn't easily allow you to
       tell a number from an indirect object reference with some
       serious looking ahead */
    
    if (*start < end && isdigit(**start)) {
      tmp1 = parse_pdf_number(start, end);
      tmp2 = NULL;
      /* This could be a # # R type reference.  We can't be sure unless
	 we look ahead for the second number and the 'R' */
      skip_white(start, end);
      position2 = *start;
      if (*start < end && isdigit(**start)) {
	tmp2 = parse_pdf_number(start, end);
      } else
	tmp2 = NULL;
      skip_white(start, end);
      if (tmp1 != NULL && tmp2 != NULL && *start < end && *((*start)++) == 'R') {
	result = pdf_new_ref ((int) pdf_number_value (tmp1), 
			      (int) pdf_number_value (tmp2));
	break;
      }
      if (tmp1 != NULL) {
	if (tmp2 != NULL)
	  pdf_release_obj (tmp2);
	*start = position2;
      }
      result = tmp1;
      break;
    }
    if (*start < end && **start == 'n') {
      result = parse_pdf_null(start, end);
      break;
    }
    result = NULL;
    break;
  }
  if (result == NULL) {
    fprintf (stderr, "\nExpecting an object, but didn't find one");
    *start = save;
    dump(*start, end);
    return NULL;
  }
  return result;
}


void pdf_parse_special(char *buffer, UNSIGNED_QUAD size, double
		       x_user, double y_user, double x_media, double y_media)
{
  int pdfmark;
  char *start = buffer, *end;
  end = buffer + size;

  if (!is_pdf_special(&start, end)) {
    fprintf (stderr, "\nNon PDF special ignored\n");
    return;
  }
  /* Must have a pdf special */
  if ((pdfmark = parse_pdfmark(&start, end)) < 0)
    {
      fprintf (stderr, "\nSpecial ignored.\n");
      return;
    }
  if (verbose)
    fprintf (stderr, "pdfmark = %d\n", pdfmark);
  switch (pdfmark) {
  case ANN:
    do_ann(&start, end);
    break;
  case OUT:
    do_outline(&start, end);
    break;
  case ARTICLE:
    do_article(&start, end);
    break;
  case BEAD:
    do_bead(&start, end);
    break;
  case DEST:
    do_dest(&start, end);
    break;
  case DOCINFO:
    do_docinfo(&start, end);
    break;
  case DOCVIEW:
    do_docview(&start, end);
    break;
  case OBJ:
    do_obj(&start, end);
    break;
  case CONTENT:
    do_content(&start, end);
    break;
  case PUT:
    do_put(&start, end);
    break;
  case CLOSE:
    do_close(&start, end);
    break;
  case BOP:
    do_bop(&start, end);
    break;
  case EOP:
    do_eop(&start, end);
    break;
  case EPDF:
    do_epdf(&start, end, x_user, y_user);
    break;
  }
}

FILE *pdf_input_file;
static int backup_line ()
{
  int ch;
  ch = 0;
  
  do {
    seek_relative (pdf_input_file, -2);
  } while (tell_position (pdf_input_file) > 0 && (ch = fgetc (pdf_input_file)) >= 0 && ch != '\n');
  if (ch < 0) {
    fprintf (stderr, "Invalid PDF file\n");
    return 1;
  }
  return 0;
}

static long xref_pos;

static long find_xref(void)
{
  long currentpos, xref_pos;
  int length;
  char *start, *end, *number;
  fprintf (stderr, "find_xref");
  seek_end (pdf_input_file);
  do {
    backup_line();
    currentpos = tell_position(pdf_input_file);
    fread (work_buffer, sizeof(char), strlen("startxref"), pdf_input_file);
    seek_absolute(pdf_input_file, currentpos);
  } while (strncmp (work_buffer, "startxref", strlen ("startxref")));
  while (fgetc (pdf_input_file) != '\n');
  fgets (work_buffer, WORK_BUFFER_SIZE, pdf_input_file);
  start = work_buffer;
  end = start+strlen(work_buffer);
  skip_white(&start, end);
  fprintf (stderr, "[%s]\n", start);
  xref_pos = (long) atof (number = parse_number (&start, end));
  fprintf(stderr, "xref is %ld", xref_pos);
  release (number);
  return xref_pos;
}

static long find_trailer(void)
{
  long currentpos;
  seek_end (pdf_input_file);
  do {
    backup_line();
    currentpos = tell_position(pdf_input_file);
    fread (work_buffer, sizeof(char), strlen("trailer"), pdf_input_file);
    seek_absolute(pdf_input_file, currentpos);
  } while (strncmp (work_buffer, "trailer", strlen ("trailer")));
  return currentpos;
}

pdf_obj *parse_trailer (char **start, char *end)
{
  if (*start >=  end || strncmp (*start, "trailer", strlen("trailer"))) {
    fprintf (stderr, "No trailer.  Are you sure this is a PDF file?\n");
    return NULL;
  }
  *start += strlen("trailer");
  skip_white(start, end);
  return (parse_pdf_dict (start, end));
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
} *xref_table;
long num_input_objects;


long trailer_pos;

long next_object (int obj)
{
  int i;
  long this_position, result = trailer_pos;  /* Worst case */
  this_position = xref_table[obj].position;
  /* Check all other objects to find next one */
  for (i=0; i<num_input_objects; i++) {
    if (xref_table[i].position > this_position &&
	xref_table[i].position < result)
      result = xref_table[i].position;
  }
  /* xref is also an object, but not in the table so
     check it separately */
  if (xref_pos > this_position &&
      xref_pos < result)
    result = xref_pos;
  return result;
}

/* The following routine returns a reference to an object existing
   only in file.  It does this as follows.  If the object
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
  fprintf (stderr, "\n\npdf_ref_file_obj: obj=%d\n", obj_no);
  if (xref_table[obj_no].indirect != NULL) {
    fprintf (stderr, "\npdf_ref_file_obj: obj=%d is already referenced\n",obj_no);
    return pdf_link_obj (xref_table[obj_no].indirect);
  }
  if ((direct = pdf_read_object (obj_no)) == NULL) {
    fprintf (stderr, "\npdf_ref_file_obj: Could not read object\n");
    return NULL;
  }
  indirect = pdf_ref_obj (direct);
  xref_table[obj_no].indirect = indirect;
  xref_table[obj_no].direct = direct;
  /* Make sure the caller can't doesn't free this object */
  pdf_write_obj (stderr, direct);
  pdf_write_obj (stderr, indirect);
  return pdf_link_obj (indirect);
}


static pdf_obj *pdf_new_ref (int label, int generation) 
{
  pdf_obj *result;
  struct pdf_indirect *indirect;
  fprintf (stderr, "\nEntered pdf_new_ref\n");
  fprintf (stderr, "num_input_objects = %d\n", num_input_objects);
  fprintf (stderr, "label=%d, gen=%d\n", label, generation);
  if (label >= num_input_objects || label < 0) {
    fprintf (stderr, "pdf_new_ref: Object doesn't exist\n");
    return NULL;
  }
  result = pdf_new_obj (PDF_INDIRECT);
  indirect = NEW (1, struct pdf_indirect);
  result -> data = indirect;
  indirect -> label = label;
  indirect -> generation = generation;
  indirect -> dirty = 1;
  return result;
}

pdf_obj *pdf_read_object (int obj_no) 
{
  long start_pos, end_pos, length;
  char *buffer, *number, *parse_pointer, *end;
  pdf_obj *result;
  fprintf (stderr, "\nEntered pdf_read: obj = %d\n", obj_no);
  if (obj_no < 0 || obj_no >= num_input_objects) {
    fprintf (stderr, "\nTrying to read nonexistent object\n");
    return NULL;
  }
  if (!xref_table[obj_no].used) {
    fprintf (stderr, "\nTrying to read deleted object\n");
    return NULL;
  }
  seek_absolute (pdf_input_file, start_pos =
		 xref_table[obj_no].position);
  end_pos = next_object (obj_no);
  fprintf (stderr, "\nstart: %d end %d\n", start_pos, end_pos);
  
  buffer = NEW (end_pos - start_pos+1, char);
  fread (buffer, sizeof(char), end_pos-start_pos, pdf_input_file);
  buffer[end_pos-start_pos] = 0;
  fprintf (stderr, "Parsing object from:\n");
  fprintf (stderr, "%s\n", buffer);
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

pdf_obj *pdf_deref_obj (pdf_obj *obj)
{
  pdf_obj *result, *tmp;
  fprintf (stderr, "\nEntered pdf_deref\n");
  pdf_write_obj (stderr, obj);
  if (obj -> type != PDF_INDIRECT) {
    fprintf(stderr, "\nreturning same obj\n");
    return pdf_link_obj (obj);
  }
  result = pdf_read_object (((struct pdf_indirect *)(obj-> data)) -> label);
  fprintf (stderr, "\nread object");
  pdf_write_obj (stderr, result);
  while (result -> type == PDF_INDIRECT) {
    fprintf(stderr, "\nderefing again\n");
    tmp = pdf_read_object (result -> label);
    pdf_write_obj (stderr, tmp);
    pdf_release_obj (result);
    result = tmp;
  }
  return result;
}

void parse_xref (void)
{
  long first_obj;
  int i, length;
  char *start, *end;
  xref_pos = find_xref();
  seek_absolute (pdf_input_file, xref_pos);
  /* Now at beginning of actual xref table */
  fgets (work_buffer, WORK_BUFFER_SIZE, pdf_input_file);
  if (strncmp (work_buffer, "xref", strlen("xref"))) {
    fprintf (stderr, "No xref.  Are you sure this is a PDF file?\n");
    return;
  }
  fgets (work_buffer, WORK_BUFFER_SIZE, pdf_input_file);
  sscanf (work_buffer, "%ld %d", &first_obj, &num_input_objects);
  xref_table = NEW (num_input_objects, struct object);
  for (i=0; i<num_input_objects; i++) {
    fread (work_buffer, sizeof(char), 20, pdf_input_file);
    work_buffer[19] = 0;
    fprintf (stderr, "[%s]\n", work_buffer);
    sscanf (work_buffer, "%ld %d", &(xref_table[i].position), 
	    &(xref_table[i].generation));
    if (work_buffer[17] != 'n' && work_buffer[17] != 'f') {
      fprintf (stderr, "PDF file is corrupt\n");
      fprintf (stderr, "[%s]\n", work_buffer);
      return;
    }
    if (work_buffer[17] == 'n')
      xref_table[i].used = 1;
    else
      xref_table[i].used = 0;
    xref_table[i].direct = NULL;
    xref_table[i].indirect = NULL;
  }
  for (i=0; i<num_input_objects; i++) {
    if (xref_table[i].used) {
      fprintf (stderr, "id:%d\tpos:%ld \tgen:%d\n", i, xref_table[i].position,
	       xref_table[i].generation);
    }
  }
}


static int num_xobjects = 0;

void pdf_open (char *file, double x_user, double y_user)
{
  long eof_pos;
  int i, done;
  pdf_obj *tmp1, *tmp2;
  pdf_obj *trailer;
  pdf_obj *catalog_ref, *catalog, *page_tree_ref, *page_tree,
    *kids_ref, *kids;
  pdf_obj *media_box, *resources, *contents, *contents_ref;
  pdf_obj *new_resources;
  pdf_obj *xobj_dict;
  char *parse_pointer, *key;
  if ((pdf_input_file = fopen (file, "r")) == NULL) {
    fprintf (stderr, "Unable to open file name %s\n", file);
    return;
  }
  seek_end(pdf_input_file);
  eof_pos = tell_position (pdf_input_file);
  fprintf (stderr, "eof @ %ld\n", eof_pos);
  parse_xref();
  trailer_pos = find_trailer();
  fprintf (stderr, "trailer @ %ld\n", trailer_pos);
  seek_absolute (pdf_input_file, trailer_pos);
  /* Let's go ahead and try to read the rest of the file into
     work_buffer.  Normally, this shouldn't be that big */
  if (eof_pos - trailer_pos >= WORK_BUFFER_SIZE) {
    fprintf (stderr, "PDF file has an unusually large trailer\n");
    fprintf (stderr, "Unable to read into work buffer\n");
    return;
  }
  fread (work_buffer, sizeof(char), eof_pos-trailer_pos,
	 pdf_input_file);
  parse_pointer = work_buffer;
  trailer = parse_trailer (&parse_pointer,
			   work_buffer+(eof_pos-trailer_pos));
  pdf_write_obj (stderr, trailer);
  /* Now just lookup catalog location */
  catalog_ref = pdf_lookup_dict (trailer, "Root");
  fprintf (stderr, "Catalog:\n");
  pdf_write_obj (stderr, catalog_ref);
  /* Deref catalog */
  catalog = pdf_deref_obj (catalog_ref);

  /* Lookup page tree in catalog */
  page_tree_ref = pdf_lookup_dict (catalog, "Pages");
  pdf_write_obj (stderr, page_tree_ref);
  page_tree = pdf_deref_obj (page_tree_ref);
  pdf_write_obj (stderr, page_tree);

  /* Media box and resources can be inherited so start looking for
     them here */
  media_box = pdf_lookup_dict (page_tree, "MediaBox");
  resources = pdf_new_dict();
  tmp1 = pdf_lookup_dict (page_tree, "Resources");
  if (tmp1) {
    pdf_merge_dict (tmp1, resources);
    pdf_release_obj (resources);
    resources = tmp1;
  }
  while ((kids_ref = pdf_lookup_dict (page_tree, "Kids")) != NULL) {
    pdf_release_obj (page_tree);
    pdf_release_obj (page_tree_ref);

    fprintf (stderr, "kids");
    pdf_write_obj (stderr, kids_ref);
    kids = pdf_deref_obj (kids_ref);
    pdf_release_obj (kids_ref);

    page_tree_ref = pdf_get_array (kids, 1);
    page_tree = pdf_deref_obj (page_tree_ref);
    pdf_release_obj (kids);

    fprintf (stderr, "new page tree ");
    pdf_write_obj (stderr, page_tree_ref);
    pdf_write_obj (stderr, page_tree);
    /* Replace MediaBox if it's here */
    tmp1 = pdf_lookup_dict (page_tree, "MediaBox");
    if (tmp1 && media_box)
      pdf_release_obj (media_box);
    if (tmp1) 
      media_box = tmp1;
    /* Add resources if they're here */
    tmp1 = pdf_lookup_dict (page_tree, "Resources");
    if (tmp1) {
      pdf_merge_dict (tmp1, resources);
      pdf_release_obj (resources);
      resources = tmp1;
    }
  }
  /* At this point, page_tree contains the first page.  media_box and
     resources should also be set. */
  contents_ref = pdf_lookup_dict (page_tree, "Contents");
  pdf_release_obj (page_tree);
  contents = pdf_deref_obj (contents_ref);
  pdf_release_obj(contents_ref);  /* Remove "old" reference */
  contents_ref = pdf_ref_obj (contents);  /* Give it a "new" reference */

  fprintf (stderr, "\n\nmediabox:"); pdf_write_obj (stderr, media_box);
  fprintf (stderr, "\n\nresources:"); pdf_write_obj (stderr,
						     resources);
  xobj_dict = pdf_stream_dict (contents);
  num_xobjects += 1;
  sprintf (work_buffer, "Fm%d", num_xobjects);
  pdf_doc_add_to_page_xobjects (work_buffer, contents_ref);
  pdf_add_dict (xobj_dict, tmp1 = pdf_new_name ("Name"),
		tmp2 = pdf_new_name (work_buffer));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);

  pdf_add_dict (xobj_dict, tmp1 = pdf_new_name ("Type"), tmp2 =
		pdf_new_name ("XObject"));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);

  fprintf (stderr, "\n\ncontents1:"); pdf_write_obj (stderr, contents);
  fprintf (stderr, "\nendcontents\n\n");

  pdf_add_dict (xobj_dict, tmp1 = pdf_new_name ("Subtype"), tmp2 =
		pdf_new_name ("Form"));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  pdf_add_dict (xobj_dict, tmp1 = pdf_new_name ("BBox"), media_box);
  pdf_release_obj (tmp1); pdf_release_obj (media_box);
  pdf_add_dict (xobj_dict, tmp1 = pdf_new_name ("FormType"), 
		tmp2 = pdf_new_number(1.0));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  tmp1 = pdf_new_array();
  pdf_add_array (tmp1, tmp2 = pdf_new_number (1));  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0));  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0));  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (1));  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0));  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0));  pdf_release_obj (tmp2);
  pdf_add_dict (xobj_dict, tmp2 = pdf_new_name ("Matrix"), tmp1);
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  
  new_resources = pdf_new_dict();
  pdf_add_dict (xobj_dict, tmp1 = pdf_new_name ("Resources"),
		tmp2 = pdf_ref_obj (new_resources));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  for (i=1; (key = pdf_get_dict(resources, i)) != NULL; i++) 
    {
      fprintf (stderr, "\nkey=%s\n", key);
      tmp1 = pdf_lookup_dict (resources, key);
      tmp2 = pdf_deref_obj (tmp1);
      fprintf (stderr, "\ndereffed obj");
      pdf_write_obj(stderr, tmp2);
      pdf_release_obj (tmp1);
      tmp1 = pdf_ref_obj (tmp2);
      pdf_release_obj (tmp2);
      pdf_add_dict (new_resources, tmp2 = pdf_new_name (key), tmp1);
      pdf_release_obj (tmp1);
      pdf_release_obj (tmp2);
      release (key);
      fprintf (stderr, "end");
      
    }
  pdf_release_obj (new_resources);
  pdf_release_obj (resources);
  fprintf (stderr, "\n\ncontents:"); pdf_write_obj (stderr, contents);
  fprintf (stderr, "Here.\n");
  sprintf (work_buffer, " q 1 0 0 1 %g %g cm /Fm%d Do Q ", x_user, y_user, num_xobjects);
  fprintf (stderr, "THere.\n");
  pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
  fprintf (stderr, "Everywhere.\n");
  pdf_release_obj(contents);
  fprintf (stderr, "???.\n");

  /* Following loop must be iterated because each write could trigger
     an additional indirect reference of an object with a lower
     number! */

  do {
    done = 1;
    for (i=0; i<num_input_objects; i++) {
      if (xref_table[i].direct != NULL) {
	fprintf (stderr, "\n\nreleasing the following object file# = %d:\n", i);
	fprintf (stderr, "\n");
	pdf_release_obj (xref_table[i].direct);
	xref_table[i].direct = NULL;
	done = 0;
      }
    }
  } while (!done);
}

