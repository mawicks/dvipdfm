/* DVI op codes */
#define SET_CHAR_0 0
#define SET_CHAR_1 1
/* etc. */
#define SET_CHAR_127 127
#define SET1   128 /* Typesets its single operand between 128 adn 255 */
#define SET2   129 /* Typesets its single two byte unsigned operand */
#define SET3   130 /* Typesets its single three byte unsigned operand */
#define SET4   131 /* Typesets its single four byte unsigned operand */
#define SET_RULE 132 /* Sets a rule of height param1(four bytes) and width param2(four bytes) */
                     /* These are *signed*.  Nothing typeset for nonpositive values */
                     /* However, negative value *do* change current point */
#define PUT1   133 /* Like SET1, but point doesn't change */
#define PUT2   134 /* Like SET2 */
#define PUT3   135 /* Like SET3 */
#define PUT4   136 /* Like SET4 */
#define PUT_RULE 137 /* Like SET_RULE */
#define NOP    138 
#define BOP    139 /* Followed by 10 four byte count registers (signed?).  Last parameter points to */
                   /* previous BOP (backward linked, first BOP has -1).  BOP clears stack and resets current point. */
#define EOP    140
#define PUSH   141 /* Pushes h,v,w,x,y,z */
#define POP    142 /* Opposite of push*/
#define RIGHT1 143 /* Move right by one byte signed operand */
#define RIGHT2 144 /* Move right by two byte signed operand */
#define RIGHT3 145 /* Move right by three byte signed operand */
#define RIGHT4 146 /* Move right by four byte signed operand */
#define W0     147 /* Move right w */
#define W1     148 /* w <- single byte signed operand.  Move right by same amount */
#define W2     149 /* Same as W1 with two byte signed operand */
#define W3     150 /* Three byte signed operand */
#define W4     151 /* Four byte signed operand */
#define X0     152 /* Move right x */
#define X1     153 /* Like W1 */
#define X2     154 /* Like W2 */
#define X3     155 /* Like W3 */
#define X4     156 /* Like W4 */
#define DOWN1  157 /* Move down by one byte signed operand */
#define DOWN2  158 /* Two byte signed operand */
#define DOWN3  159 /* Three byte signed operand */
#define DOWN4  160 /* Four byte signed operand */
#define Y0     161 /* Move down by y */
#define Y1     162 /* Move down by one byte signed operand, which replaces Y */
#define Y2     163 /* Two byte signed operand */
#define Y3     164 /* Three byte signed operand */
#define Y4     165 /* Four byte signed operand */
#define Z0     166 /* Like Y0, but use z */
#define Z1     167 /* Like Y1 */
#define Z2     168 /* Like Y2 */
#define Z3     169 /* Like Y3 */
#define Z4     170 /* Like Y4 */
#define FNT_NUM_0 171 /* Switch to font 0 */
#define FNT_NUM_1 172 /* Switch to font 1 */
/* etc. */
#define FNT_NUM_63 234 /* Switch to font 63 */
#define FNT1       235 /* Switch to font described by single byte unsigned operand */
#define FNT2       236 /* Switch to font described by two byte unsigned operand */
#define FNT3       237 /* Three byte font descriptor */
#define FNT4       238 /* Four byte operator (Knuth says signed, but what would be the point? */
#define XXX1       239 /* Special.  Operand is one byte length.  Special follows immediately */
#define XXX2       240 /* Two byte operand */
#define XXX3       241 /* Three byte operand */ 
#define XXX4       242 /* Four byte operand (Knuth says TeX uses only XXX1 and XXX4 */
#define FNT_DEF1  243 /* One byte font number, four byte checksum, four byte magnified size (DVI units),
                          four byte designed size, single byte directory length, single byte name length,
                          followed by complete name (area+name) */
#define FNT_DEF2  244 /* Same for two byte font number */
#define FNT_DEF3  245 /* Same for three byte font number */
#define FNT_DEF4  246 /* Four byte font number (Knuth says signed) */
#define PRE        247 /* Preamble:
                              one byte DVI version (should be 2)
                              four byte unsigned numerator
                              four byte unsigned denominator -- one DVI unit = den/num*10^(-7) m
                              four byte magnification (multiplied by 1000)
                              one byte unsigned comment length followed by comment. */
#define DVI_ID             2    /* ID Byte for current DVI file */
#define POST       248  /* Postamble- -- similar to preamble
                              four byte pointer to final bop
                              four byte numerator
                              four byte denominator
                              four byte mag
                              four byte maximum height (signed?)
                              four byte maximum width 
                              two byte max stack depth required to process file
                              two byte number of pages */
#define POST_POST  249  /* End of postamble
                              four byte pointer to POST command
                              Version byte (same as preamble)
                              Padded by four or more 223's to the end of the file. */
#define PADDING    223

/* Font definitions appear between POST and POST_POST */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "numbers.h"
#include "io.h"
#include "system.h"
#include "dvi_limits.h"

/* External functions defined in this file:
      dvi_set_verbose ()   - Enable verbose progress reporting
      dvi_open(filename)   - Open a dvi file and initialize
                             data structures to represent file.
      dvi_close()          - Close a dvi file
  
   Static functions defined in this file:
      invalid_signature ()  - Error handler for noncomforming DVI file.
      find_post_post(file) - Find the post_post signature in a DVI file. */


/* Interal Variables */

static FILE *dvi_file;
static dvi_verbose = 0;
static unsigned numfonts = 0, stackdepth;
static unsigned long page_loc[MAX_PAGES];
static char dvi_comment[257];
static unsigned long post_location, dvi_file_size;
static UNSIGNED_PAIR numpages = 0;
static UNSIGNED_QUAD media_width, media_height;
static UNSIGNED_QUAD dvi_unit_num, dvi_unit_den, dvi_mag;

struct font_def {
  signed long id;
  unsigned long checksum, size, design_size;
  char *directory, *name;
} font_def[MAX_FONTS];

static num, den, mag;

static void invalid_signature()
{
  ERROR ("dvi_open:  Something is wrong.  Are you sure this is a DVI file?\n");
}

#define range_check_loc(loc) {if ((loc) > dvi_file_size) invalid_signature();}



void dvi_set_verbose(void)
{
  dvi_verbose = 1;
}

static void find_post (void)
{
  long current;
  int read_byte;

  /* First find end of file */  
  dvi_file_size = file_size (dvi_file);
  current = dvi_file_size;
 
  /* Scan backwards through PADDING */  
  do {
     current -= 1;
     seek_absolute (dvi_file, current);

  } while ((read_byte = fgetc(dvi_file)) == PADDING &&
	   current > 0);

  /* file_position now points to last non padding character or beginning of file */
  if (dvi_file_size - current < 4 ||
      current == 0 || read_byte != DVI_ID) {
    fprintf (stderr, "DVI ID = %d\n", read_byte);
    invalid_signature();
  } 

  /* Make sure post_post is really there */
  current = current - 5;
  seek_absolute (dvi_file, current);
  if ((read_byte = fgetc(dvi_file)) != POST_POST) {
     fprintf (stderr, "Found %d where post_post opcode should be\n", read_byte);
     invalid_signature();
  }
  if (dvi_verbose) {
    fprintf (stderr, "Post_post:\t %8ld\n", current);
  }

  current = get_signed_quad (dvi_file);
  seek_absolute (dvi_file, current);
  if ((read_byte = fgetc(dvi_file)) != POST) {
     fprintf (stderr, "Found %d where post_post opcode should be\n", read_byte);
     invalid_signature();
  }
  post_location = current;
  if (dvi_verbose) {
    fprintf (stderr, "Post:     \t %8ld\n", post_location);
  }
}

static void get_page_info (void) 
{
  int i;
  seek_absolute (dvi_file, post_location+27);
  numpages = get_unsigned_pair (dvi_file);
  if (dvi_verbose || numpages > MAX_PAGES) {
    fprintf (stderr, "Page count:\t %4d\n", numpages);
  }
  if (numpages > MAX_PAGES) {
    ERROR ("dvi_open:  Sorry, too many pages!");
  }
  if (numpages == 0) {
    ERROR ("dvi_open:  Page count is 0!");
  }
  seek_absolute (dvi_file, post_location+1);
  page_loc[numpages-1] = get_unsigned_quad(dvi_file);
  range_check_loc(page_loc[numpages-1]+41);
  for (i=numpages-2; i>=0; i--) {
    seek_absolute (dvi_file, page_loc[i+1]+41);
    page_loc[i] = get_unsigned_quad(dvi_file);
    range_check_loc(page_loc[numpages-1]+41);
  }
  if (dvi_verbose) {
    for (i=0; i<numpages; i++) 
      fprintf (stderr, "Page %4d:\t %8ld\n", i, page_loc[i]);
  }
}

static void get_dvi_info (void)
{
  seek_absolute (dvi_file, post_location+5);
  dvi_unit_num = get_unsigned_quad(dvi_file);
  dvi_unit_den = get_unsigned_quad(dvi_file);
  dvi_mag = get_unsigned_quad(dvi_file);
  media_height = get_unsigned_quad(dvi_file);
  media_width = get_unsigned_quad(dvi_file);
  stackdepth = get_unsigned_pair(dvi_file);
  if (stackdepth > DVI_STACK_DEPTH) {
    fprintf (stderr, "DVI needs stack depth of %d,", stackdepth);
    fprintf (stderr, "but DVI_STACK_DEPTH is %d", DVI_STACK_DEPTH);
    ERROR ("Capacity exceeded.");
  }

  if (dvi_verbose) {
    fprintf (stderr, "DVI File Info\n");
    fprintf (stderr, "Unit: %ld / %ld\n", dvi_unit_num, dvi_unit_den);
    fprintf (stderr, "Mag: %ld\n", dvi_mag);
    fprintf (stderr, "Media Height: %ld\n", media_height);
    fprintf (stderr, "Media Width: %ld\n", media_width);
    fprintf (stderr, "Stack Depth: %d\n", stackdepth);
  }
  
}


static void dump_font_info (void)
{
  unsigned i;
  for (i=0; i<numfonts; i++) {
      fprintf (stderr, "Font: (%s)/", font_def[i].directory);
      fprintf (stderr, "%s, ", font_def[i].name);
      fprintf (stderr, "ID=%ld, ", font_def[i].id);
      fprintf (stderr, "size= %ld @ ", font_def[i].design_size);
      fprintf (stderr, "%ld\n", font_def[i].size);
  }
}

static void get_a_font_record (struct font_def * a_font)
{
  UNSIGNED_BYTE dir_length, name_length;
  a_font -> checksum = get_unsigned_quad (dvi_file);
  a_font -> size = get_unsigned_quad (dvi_file);
  a_font -> design_size = get_unsigned_quad (dvi_file);
  dir_length = get_unsigned_byte (dvi_file);
  name_length = get_unsigned_byte (dvi_file);
  a_font -> directory = malloc (dir_length+1);
  a_font -> name = malloc (name_length+1);
  if (fread (a_font -> directory, 1, dir_length, dvi_file) !=
      dir_length) {
    invalid_signature();
  }
  if (fread (a_font -> name, 1, name_length, dvi_file) !=
      name_length) {
    invalid_signature();
  }
  (a_font -> directory)[dir_length] = 0;
  (a_font -> name)[name_length] = 0;
  if (dvi_verbose)
    fprintf (stderr, "[%s]", a_font -> name);
}


static int font_id_compare (const void *font1,
			    const void *font2) 
{
  const struct font_def *fd1=font1, *fd2=font2;
  if (fd1 -> id > fd2 -> id)
    return 1;
  if (fd1 -> id < fd2 -> id)
    return -1;
  if (fd1 -> id == fd2 -> id)
    return -1;
}

static void sort_font_info (void)
{
  qsort (font_def, numfonts, sizeof *font_def, font_id_compare);
}


static void get_font_info (void)
{
  UNSIGNED_BYTE code;
  if (dvi_verbose)
    fprintf(stderr, "Fonts..");
  seek_absolute (dvi_file, post_location+29);
  while ((code = get_unsigned_byte(dvi_file)) != POST_POST) {
    switch (code)
      {
      case FNT_DEF1:
	font_def[numfonts].id = get_unsigned_byte (dvi_file);
	break;
      case FNT_DEF2:
	font_def[numfonts].id = get_unsigned_pair (dvi_file);
	break;
      case FNT_DEF3:
	font_def[numfonts].id = get_unsigned_triple (dvi_file);
	break;
      case FNT_DEF4:
	font_def[numfonts].id = get_signed_quad (dvi_file);
	break;
      default:
	fprintf (stderr, "Unexpected op code: %3d\n", code);
	invalid_signature();
      }
    get_a_font_record(&font_def[numfonts]);
    numfonts += 1;
  }
  if (dvi_verbose) {
    fprintf (stderr, "\nRead %d fonts.\n", numfonts);
  }
  sort_font_info();
}

void get_comment(void)
{
  UNSIGNED_BYTE length;
  seek_absolute (dvi_file, 14);
  length = get_unsigned_byte(dvi_file);
  if (fread (dvi_comment, 1, length, dvi_file) != length) {
    invalid_signature();
  }
  dvi_comment[length] = 0;
  if (dvi_verbose) {
    fprintf (stderr, "Comment: %s\n", dvi_comment);
  }
}


error_t dvi_open (char * filename)
{
  long current;
  if (!(dvi_file = fopen (filename, "r"))) {
    ERROR ("dvi_open:  Specified DVI file doesn't exist");
    return (FATAL_ERROR);
  }
  /* DVI files are most easily read backwards by searching
     for post_post and then post opcode */
  find_post ();

  get_dvi_info();
  get_page_info();
  get_font_info();
  get_comment();
}

error_t dvi_close (void)
{
  int i;
  
  /* Do some house cleaning */
  fclose (dvi_file);
  for (i=0; i<numfonts; i++) {
    free (font_def[i].directory);
    free (font_def[i].name);
  }
  numfonts = 0;
  numpages = 0;
  dvi_file = NULL;
}



