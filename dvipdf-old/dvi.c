/* DVI op codes */
#define SET_CHAR_0 0
#define SET_CHAR_1 1
/* etc. */
#define SET_CHAR_127 127
#define SET1   128 /* Typesets its single operand between 128 and 255 */
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
#include <ctype.h>
#include "error.h"
#include "numbers.h"
#include "io.h"
#include "system.h"
#include "dvi_limits.h"
#include "pdfdev.h"
#include "tfm.h"
#include "mem.h"

/* External functions defined in this file:
      dvi_set_verbose ()   - Enable verbose progress reporting
      dvi_set_debug ()   - Enable verbose progress reporting
      dvi_open(filename)   - Open a dvi file and initialize
                             data structures to represent file.
      int dvi_npages()     - Returns the total number of pages in the
      			     dvi file.
      dvi_close()          - Close a dvi file
      void dvi_do_page(n)  - Process page #n (First page is 0!)
  
   Static functions defined in this file:
      invalid_signature ()  - Error handler for noncomforming DVI file.
      find_post_post(file) - Find the post_post signature in a DVI file. */


/* Interal Variables */

static FILE *dvi_file;
static dvi_verbose = 0;
static dvi_debug = 0;
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
  int tfm_id;
} font_def[MAX_FONTS];

static void invalid_signature()
{
  ERROR ("dvi_open:  Something is wrong.  Are you sure this is a DVI file?\n");
}

#define range_check_loc(loc) {if ((loc) > dvi_file_size) invalid_signature();}



void dvi_set_verbose(void)
{
  dvi_verbose = 1;
}

void dvi_set_debug(void)
{
  dvi_debug = 1;
}

int dvi_npages (void)
{
  return numpages;
}

static void find_post (void)
{
  long current;
  int read_byte;

  if (dvi_debug) {
    fprintf (stderr, "dvi_open: Searching for post\n");
  }

  /* First find end of file */  
  dvi_file_size = file_size (dvi_file);
  if (dvi_debug) {
    fprintf (stderr, "dvi_open: DVI size is %ld\n", dvi_file_size);
  }
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

double dvi_tell_mag (void)
{
  return (dvi_mag/1000.0);
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
  if (stackdepth > DVI_MAX_STACK_DEPTH) {
    fprintf (stderr, "DVI needs stack depth of %d,", stackdepth);
    fprintf (stderr, "but MAX_DVI_STACK_DEPTH is %d", DVI_MAX_STACK_DEPTH);
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
    dump_font_info ();
    fprintf (stderr, "\nRead %d fonts.\n", numfonts);
  }
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

/* The section below this line deals with the actual processing of the
   dvi file.

   The dvi file processor state is contained in the following
   variables: */

struct dvi_registers {
  SIGNED_QUAD h, v, w, x, y, z;
};

static struct dvi_registers dvi_state;
static struct dvi_registers dvi_stack[DVI_MAX_STACK_DEPTH];
static int current_font;
static dvi_stack_depth = 0;  
static int processing_page = 0;

/* Following are computed "constants" used for unit conversion */

static double dvi2dev_x, dvi2dev_y;
static double dvi2pts;

static void clear_state (void)
{
  dvi_state.h = 0; dvi_state.v = 0; dvi_state.w = 0;
  dvi_state.x = 0; dvi_state.y = 0; dvi_state.z = 0;
  dvi_stack_depth = 0;
  current_font = -1;
}

static void do_scales (void)
{
  unsigned long dev_xres, dev_yres;
  dev_xres = dev_tell_xdpi();
  dev_yres = dev_tell_ydpi();
  clear_state();
  dvi2dev_x = (double) dvi_unit_num / (double) dvi_unit_den;
  dvi2dev_x *= (double) dvi_mag / 1000.0;
  dvi2dev_y = dvi2dev_x;
  dvi2dev_x *= (double) dev_xres/254000.0;
  dvi2dev_y *= (double) dev_yres/254000.0;
  dvi2pts = (double) dvi_unit_num / (double) dvi_unit_den;
  dvi2pts *= (72.0)/(254000.0);
  dvi2pts *= (double) dvi_mag / 1000.0;
}

static void do_locate_fonts (void) 
{
  int i, j;
  for (i=0; i<numfonts; i++) {
    fprintf (stderr, "\n<%s @ %gpt>",
	     font_def[i].name, ROUND(font_def[i].size*dvi2pts,0.1));
    /* Only need to read tfm once for the same name.  Check to see
       if it already exists */
    for (j=0; j<i; j++) {
      if (!strcmp (font_def[i].name, font_def[j].name)) 
	break;
    }
    if (j == i)
      font_def[i].tfm_id = tfm_open (font_def[i].name);
    else
      font_def[i].tfm_id = font_def[j].tfm_id;
    dev_locate_font (font_def[i].name, font_def[i].id, 
		     font_def[i].tfm_id,
		     font_def[i].size*dvi2pts);
  }
}


void dvi_init (char *outputfile)
{
  if (dvi_debug) fprintf (stderr, "dvi: computing scaling parameters\n");
  do_scales();
  if (dvi_debug) fprintf (stderr, "dvi: Initializing output device\n");
  dev_init (outputfile);
  if (dvi_debug) fprintf (stderr, "dvi: locating fonts\n");
  do_locate_fonts();
  if (dvi_debug) fprintf (stderr, "dvi: pdf_init done\n");
}

void dvi_complete (void)    
{
  if (dvi_debug) fprintf (stderr, "dvi:  Closing output device...");
  dev_close();
  if (dvi_debug) fprintf (stderr, "dvi:  Output device closed\n");
}


static void do_moveto (SIGNED_QUAD x, SIGNED_QUAD y)
{
  dvi_state.h = x;
  dvi_state.v = y;
  if (dvi_debug) fprintf (stderr, "do_moveto: x = %ld, y = %ld\n", x, y);
  dev_moveto (x*dvi2dev_x, y*dvi2dev_y);
}

static void do_right (SIGNED_QUAD ch)
{
  dvi_state.h += ch;
  dev_moveright (ch*dvi2dev_x);
  /*  do_moveto (dvi_state.h, dvi_state.v); */
}

static void do_down (SIGNED_QUAD ch)
{
  dvi_state.v += ch;
  do_moveto (dvi_state.h, dvi_state.v);
}


static void do_set (SIGNED_QUAD ch)
{
  double dvi_width, dev_width;
  if (current_font < 0) {
    ERROR ("do_set:  No font selected");
  }
  /*  dvi_width = tfm_get_width (font_def[current_font].tfm_id, ch) * font_def[current_font].design_size; */
  dvi_width = tfm_get_width (font_def[current_font].tfm_id, ch) * font_def[current_font].size;
  dvi_state.h += dvi_width;
  dev_set_char (ch, dvi_width*dvi2dev_x);
  if (dvi_debug) {
    fprintf (stderr, "Done\n");
  }
  /* Move right by the character width */
}

static void do_rule (SIGNED_QUAD width, SIGNED_QUAD height)
{
  do_moveto (dvi_state.h, dvi_state.v);
  dev_rule (width*dvi2dev_x, height*dvi2dev_y);
}

static void do_set1(void)
{
  do_set (get_unsigned_byte(dvi_file));
}

static void do_setrule(void)
{
  SIGNED_QUAD width, height;
  height = get_signed_quad (dvi_file);
  width = get_signed_quad (dvi_file);
  if (width > 0 && height > 0) {
    do_rule (width, height);
  }
  do_right (width);
}

static void do_putrule(void)
{
  SIGNED_QUAD width, height;
  height = get_signed_quad (dvi_file);
  width = get_signed_quad (dvi_file);
  if (width > 0 && height > 0) {
    do_moveto (dvi_state.h, dvi_state.v);
    do_rule (width, height);
    do_moveto (dvi_state.h, dvi_state.v); 
  }
}

static void do_put (SIGNED_QUAD ch)
{
  dev_set_char (ch, 0.0);
  do_moveto (dvi_state.h, dvi_state.v); /* Move back */
}

static void do_put1(void)
{
  do_set (get_unsigned_byte(dvi_file));
}

static void do_push (void) 
{
  if (dvi_debug) {
    fprintf (stderr, "Pushing onto stack of depth %d\n",
	     dvi_stack_depth);
  }
  dvi_stack[dvi_stack_depth++] = dvi_state;
}

static void do_pop (void)
{
  if (dvi_debug) {
    fprintf (stderr, "Popping off stack of depth %d\n",
	     dvi_stack_depth);
  }
  if (dvi_stack_depth > 0) {
    dvi_state = dvi_stack[--dvi_stack_depth];
  } else
    ERROR ("dvi_do_pop: Tried to pop an empty stack");
  do_moveto (dvi_state.h, dvi_state.v);
}


static void do_right1(void)
{
  do_right (get_signed_byte(dvi_file));
}

static void do_right2(void)
{
  do_right (get_signed_pair(dvi_file));
}

static void do_right3(void)
{
  do_right (get_signed_triple(dvi_file));
}

static void do_right4(void)
{
  do_right (get_signed_quad(dvi_file));
}

static void do_w (SIGNED_QUAD ch)
{
  dvi_state.w = ch;
  do_right (ch);
}

static void do_w0(void)
{
  do_right (dvi_state.w);
}

static void do_w1(void)
{
  do_w (get_signed_byte(dvi_file));
}

static void do_w2(void)
{
  do_w (get_signed_pair(dvi_file));
}

static void do_w3(void)
{
  do_w (get_signed_triple(dvi_file));
}

static void do_w4(void)
{
  do_w (get_signed_quad(dvi_file));
}

static void do_x (SIGNED_QUAD ch)
{
  dvi_state.x = ch;
  do_right (ch);
}

static void do_x0(void)
{
  do_right (dvi_state.x);
}

static void do_x1(void)
{
  do_x (get_signed_byte(dvi_file));
}

static void do_x2(void)
{
  do_x (get_signed_pair(dvi_file));
}

static void do_x3(void)
{
  do_x (get_signed_triple(dvi_file));
}

static void do_x4(void)
{
  do_x (get_signed_quad(dvi_file));
}

static void do_down1(void)
{
  do_down (get_signed_byte(dvi_file));
}

static void do_down2(void)
{
  do_down (get_signed_pair(dvi_file));
}

static void do_down3(void)
{
  do_down (get_signed_triple(dvi_file));
}

static void do_down4(void)
{
  do_down (get_signed_quad(dvi_file));
}

static void do_y (SIGNED_QUAD ch)
{
  dvi_state.y = ch;
  do_down (ch);
}

static void do_y0(void)
{
  do_down (dvi_state.y);
}

static void do_y1(void)
{
  do_y (get_signed_byte(dvi_file));
}

static void do_y2(void)
{
  do_y (get_signed_pair(dvi_file));
}

static void do_y3(void)
{
  do_y (get_signed_triple(dvi_file));
}

static void do_y4(void)
{
  do_y (get_signed_quad(dvi_file));
}

static void do_z (SIGNED_QUAD ch)
{
  dvi_state.z = ch;
  do_down (ch);
}

static void do_z0(void)
{
  do_down (dvi_state.z);
}

static void do_z1(void)
{
  do_z (get_signed_byte(dvi_file));
}

static void do_z2(void)
{
  do_z (get_signed_pair(dvi_file));
}

static void do_z3(void)
{
  do_z (get_signed_triple(dvi_file));
}

static void do_z4(void)
{
  do_z (get_signed_quad(dvi_file));
}

static void do_fntdef(void)
{
  int area_len, name_len, i;
  get_signed_quad(dvi_file);
  get_signed_quad(dvi_file);
  get_signed_quad(dvi_file);
  area_len = get_unsigned_byte(dvi_file);
  name_len = get_unsigned_byte(dvi_file);
  for (i=0; i<area_len+name_len; i++) {
    get_unsigned_byte (dvi_file);
  }
}

static void do_fntdef1(void)
{
  get_unsigned_byte(dvi_file);
  do_fntdef();
}

static void do_fntdef2(void)
{
  get_unsigned_pair(dvi_file);
  do_fntdef();
}

static void do_fntdef3(void)
{
  get_unsigned_triple(dvi_file);
  do_fntdef();
}

static void do_fntdef4(void)
{
  get_signed_quad(dvi_file);
  do_fntdef();
}

static void do_fnt (SIGNED_QUAD font_id)
{
  int i;
  for (i=0; i<numfonts; i++) {
    if (font_def[i].id == font_id) break;
  }
  if (i == numfonts) {
    fprintf (stderr, "fontid: %ld\n", font_id);
    ERROR ("dvi_do_fnt:  Tried to select a font that hasn't been defined");
  }
  current_font = i;
  dev_select_font (font_def[i].id);
}

static void do_fnt1(void)
{
  SIGNED_QUAD font;
  font = get_unsigned_byte(dvi_file);
  do_fnt(font);
}

static void do_fnt2(void)
{
  SIGNED_QUAD font;
  font = get_unsigned_pair(dvi_file);
  do_fnt(font);
}

static void do_fnt3(void)
{
  SIGNED_QUAD font;
  font = get_unsigned_triple(dvi_file);
  do_fnt(font);
}

static void do_fnt4(void)
{
  SIGNED_QUAD font;
  font = get_signed_quad(dvi_file);
  do_fnt(font);
}

static void do_xxx(UNSIGNED_QUAD size) 
{
  UNSIGNED_QUAD i;
  char *buffer;
  buffer = NEW (size+1, char);
  for (i=0; i<size; i++) {
    buffer[i] = get_unsigned_byte(dvi_file);
  }
  dev_do_special (buffer, size);
  release (buffer);
}

static void do_xxx1(void)
{
  SIGNED_QUAD size;
  size = get_unsigned_byte(dvi_file);
  do_xxx(size);
}

static void do_xxx2(void)
{
  SIGNED_QUAD size;
  size = get_unsigned_pair(dvi_file);
  do_xxx(size);
}

static void do_xxx3(void)
{
  SIGNED_QUAD size;
  size = get_unsigned_triple(dvi_file);
  do_xxx(size);
}

static void do_xxx4(void)
{
  SIGNED_QUAD size;
  size = get_unsigned_quad(dvi_file);
  do_xxx(size);
}

static void do_bop(void)
{
  int i;
  if (processing_page) 
    ERROR ("dvi_do_bop:  Got a bop inthe middle of a page");
  /* For now, ignore TeX's count registers */
  for (i=0; i<10; i++) {
    get_signed_quad (dvi_file);
  }
/*  Ignore previous page pointer since we have already saved this
    information */
  get_signed_quad (dvi_file);
  clear_state();
  processing_page = 1;
  dev_bop();
}

static void do_eop(void)
{
  processing_page = 0;
  if (dvi_stack_depth != 0) {
    ERROR ("do_eop:  stack_depth is not zero at end of page");
  }
  dev_eop();
}


void dvi_do_page(int n)  /* Most of the work of actually interpreting
			    the dvi file is here. */
{
  UNSIGNED_BYTE opcode;
  /* Position to beginning of page */
  if (dvi_debug) fprintf (stderr, "Seeking to page %d @ %ld\n", n,
			  page_loc[n]);
  seek_absolute (dvi_file, page_loc[n]);
  dvi_stack_depth = 0;
  while (1) {
    opcode = read_byte (dvi_file);
    if (dvi_debug) {
      fprintf (stderr, "Opcode: %d", opcode);
      if (isprint (opcode)) fprintf (stderr, " (%c)\n", opcode);
      else  fprintf (stderr, "\n");
    }
    switch (opcode)
      {
      case SET1:
	do_set1();
	break;
      case SET2:
      case SET3:
      case SET4:
	ERROR ("dvi_do_page: Multibyte byte character in DVI file.  I can't handle this!");
	break;
      case SET_RULE:
	do_setrule();
	break;
      case PUT1:
	do_put1();
	break;
      case PUT2:
      case PUT3:
      case PUT4:
	ERROR ("dvi_do_page: Multibyte byte character in DVI file.  I can't handle this!");
	break;
      case PUT_RULE:
	do_putrule();
	break;
      case NOP:
	break;
      case BOP:
	do_bop();
	break;
      case EOP:
	do_eop();
	return;
      case PUSH:
	do_push();
	break;
      case POP:
	do_pop();
	break;
      case RIGHT1:
	do_right1();
	break;
      case RIGHT2:
	do_right2();
	break;
      case RIGHT3:
	do_right3();
	break;
      case RIGHT4:
	do_right4();
	break;
      case W0:
	do_w0();
	break;
      case W1:
	do_w1();
	break;
      case W2:
	do_w2();
	break;
      case W3:
	do_w3();
	break;
      case W4:
	do_w4();
	break;
      case X0:
	do_x0();
	break;
      case X1:
	do_x1();
	break;
      case X2:
	do_x2();
	break;
      case X3:
	do_x3();
	break;
      case X4:
	do_x4();
	break;
      case DOWN1:
	do_down1();
	break;
      case DOWN2:
	do_down2();
	break;
      case DOWN3:
	do_down3();
	break;
      case DOWN4:
	do_down4();
	break;
      case Y0:
	do_y0();
	break;
      case Y1:
	do_y1();
	break;
      case Y2:
	do_y2();
	break;
      case Y3:
	do_y3();
	break;
      case Y4:
	do_y4();
	break;
      case Z0:
	do_z0();
	break;
      case Z1:
	do_z1();
	break;
      case Z2:
	do_z2();
	break;
      case Z3:
	do_z3();
	break;
      case Z4:
	do_z4();
	break;
      case FNT1:
	do_fnt1();
	break;
      case FNT2:
	do_fnt2();
	break;
      case FNT3:
	do_fnt3();
	break;
      case FNT4:
	do_fnt4();
	break;
      case XXX1:
	do_xxx1();
	break;
      case XXX2:
	do_xxx2();
	break;
      case XXX3:
	do_xxx3();
	break;
      case XXX4:
	do_xxx4();
	break;
      case FNT_DEF1:
	do_fntdef1();
	break;
      case FNT_DEF2:
	do_fntdef2();
	break;
      case FNT_DEF3:
	do_fntdef3();
	break;
      case FNT_DEF4:
	do_fntdef4();
	break;
      case PRE:
      case POST:
      case POST_POST:
	ERROR("Unexpected preamble or postamble in dvi file");
	break;
      default:
	/* One day when these were commented out, some of the doc_
	   stuff core dumped, possibly because there were no
	   characters or fonts on the page.  Some day make sure an empty page
	   doesn't core dump */
	if (opcode >= SET_CHAR_0 && opcode <= SET_CHAR_127) {
	  do_set (opcode);
	} else if (opcode >= FNT_NUM_0 && opcode <= FNT_NUM_63) {
	  do_fnt (opcode - FNT_NUM_0);
	}
      }
  }
}





