#include "numbers.h"
#include "dvi_limits.h"
#include "error.h"
#include "io.h"
#include "mem.h"

static tfm_verbose = 0;

/* TFM Record structure
     Multiple TFM's may be read in at once */

struct a_tfm
{
  UNSIGNED_PAIR wlenfile;
  UNSIGNED_PAIR wlenheader;
  UNSIGNED_PAIR bc;
  UNSIGNED_PAIR ec;
  UNSIGNED_PAIR nwidths;
  UNSIGNED_PAIR nheights;
  UNSIGNED_PAIR ndepths;
  UNSIGNED_PAIR nitcor;
  UNSIGNED_PAIR nlig;
  UNSIGNED_PAIR nkern;
  UNSIGNED_PAIR nextens;
  UNSIGNED_PAIR nfonparm;
  SIGNED_PAIR *header;
  UNSIGNED_PAIR *char_info;
  SIGNED_PAIR *width;
  SIGNED_PAIR *height;
  SIGNED_PAIR *depth;
  SIGNED_PAIR *italic;
  SIGNED_PAIR *lig_kern;
  SIGNED_PAIR *kern;
  SIGNED_PAIR *exten;
  SIGNED_PAIR *param;
};

struct a_tfm tfm[MAX_FONTS];
static numtfms = 0; /* numtfms should equal numfonts in dvi.c */

static invalid_tfm_file (void)
{
  ERROR ("tfm_open: Something is wrong.  Are you sure this is a valid TFM file?\n");
  
}


/* External Routine */

void tfm_set_verbose (void)
{
  tfm_verbose = 1;
}


FILE *tfm_file;
unsigned tfm_file_size;

static sum_of_tfm_sizes (struct a_tfm *a_tfm)
{
  unsigned long result = 6;
  result += (a_tfm -> ec - a_tfm -> bc + 1);
  result += a_tfm -> wlenheader;
  result += a_tfm -> nwidths;
  result += a_tfm -> nheights;
  result += a_tfm -> ndepths;
  result += a_tfm -> nitcor;
  result += a_tfm -> nlig;
  result += a_tfm -> nkern;
  result += a_tfm -> nextens;
  result += a_tfm -> nfonparm;
  return result;
}


static get_sizes (struct a_tfm *a_tfm)
{
  a_tfm -> wlenfile = get_unsigned_pair (tfm_file);
  a_tfm -> wlenheader = get_unsigned_pair (tfm_file);
  a_tfm -> bc = get_unsigned_pair (tfm_file);
  a_tfm -> ec = get_unsigned_pair (tfm_file);
  if (a_tfm -> ec < a_tfm -> bc)
    invalid_tfm_file();
  a_tfm -> nwidths = get_unsigned_pair (tfm_file);
  a_tfm -> nheights = get_unsigned_pair (tfm_file);
  a_tfm -> ndepths = get_unsigned_pair (tfm_file);
  a_tfm -> nitcor = get_unsigned_pair (tfm_file);
  a_tfm -> nlig = get_unsigned_pair (tfm_file);
  a_tfm -> nkern = get_unsigned_pair (tfm_file);
  a_tfm -> nextens = get_unsigned_pair (tfm_file);
  a_tfm -> nfonparm = get_unsigned_pair (tfm_file);
  if (4 * (a_tfm -> wlenfile) != tfm_file_size ||
      sum_of_tfm_sizes (a_tfm) != a_tfm -> wlenfile)
    invalid_tfm_file();
  fprintf (stderr, "Computed size %d\n", sum_of_tfm_sizes (a_tfm));
  fprintf (stderr, "Stated size %d\n", a_tfm -> wlenfile);
  fprintf (stderr, "Actual size %d\n", tfm_file_size);
}

static dump_sizes (struct a_tfm *a_tfm)
{
  fprintf (stderr, "wlenfile: %d\n", a_tfm -> wlenfile);
  fprintf (stderr, "wlenheader: %d\n", a_tfm -> wlenheader);
  fprintf (stderr, "bc: %d\n", a_tfm -> bc);
  fprintf (stderr, "ec: %d\n", a_tfm -> ec);
  fprintf (stderr, "nwidths: %d\n", a_tfm -> nwidths);
  fprintf (stderr, "nheights: %d\n", a_tfm -> nheights);
  fprintf (stderr, "ndepths: %d\n", a_tfm -> ndepths);
  fprintf (stderr, "nitcor: %d\n", a_tfm -> nitcor);
  fprintf (stderr, "nlig: %d\n", a_tfm -> nlig);
  fprintf (stderr, "nkern: %d\n", a_tfm -> nkern);
  fprintf (stderr, "nextens: %d\n", a_tfm -> nextens);
  fprintf (stderr, "nfonparm: %d\n", a_tfm -> nfonparm);
}


static get_fix_word_array (SIGNED_PAIR *a_word, int length)
{
  unsigned i;
  for (i=0; i< length; i++) {
    a_word[i] = get_signed_pair (tfm_file);
  }
}

static get_unsigned_pair_array (UNSIGNED_PAIR *a_word, int length)
{
  unsigned i;
  for (i=0; i< length; i++) {
    a_word[i] = get_unsigned_pair (tfm_file);
  }
}

static do_fix_word_array (SIGNED_PAIR *a, UNSIGNED_PAIR len)
{
  a = new (sizeof (*a) * len);
  get_fix_word_array (a, len);
}

static do_unsigned_pair_array (UNSIGNED_PAIR *a, UNSIGNED_PAIR len)
{
  a = new (sizeof (*a) * len);
  get_unsigned_pair_array (a, len);
}

static get_arrays (struct a_tfm *a_tfm)
{
  do_fix_word_array (a_tfm -> header, a_tfm -> wlenheader);
  do_unsigned_pair_array (a_tfm -> char_info, (a_tfm->ec)-(a_tfm->bc)+1);
  do_fix_word_array (a_tfm -> width, a_tfm -> nwidths);
  do_fix_word_array (a_tfm -> height, a_tfm -> nheights);
  do_fix_word_array (a_tfm -> depth, a_tfm -> ndepths);
  do_fix_word_array (a_tfm -> italic, a_tfm -> nitcor);
  do_fix_word_array (a_tfm -> lig_kern, a_tfm -> nlig);
  do_fix_word_array (a_tfm -> kern, a_tfm -> nkern);
  do_fix_word_array (a_tfm -> exten, a_tfm -> nextens);
  do_fix_word_array (a_tfm -> param, a_tfm -> nfonparm);
}

static get_tfm (struct a_tfm *a_tfm)
{
  get_sizes (a_tfm);
  get_arrays (a_tfm);
}

/* External Routine */

void tfm_open (char *tfm_file_name)
{
  if (numtfms >= MAX_FONTS) {
    ERROR ("tfm_open:  Tried to open too many TFM files!");
  }
  
  if (!(tfm_file = fopen (tfm_file_name, "r"))) {
    fprintf (stderr, "tfm_open: %s\n", tfm_file_name);
    ERROR ("tfm_open:  Specified TFM file cannot be opened");
  }
  if ((tfm_file_size = file_size(tfm_file)) < 24) {
    invalid_tfm_file ();
  }
  get_tfm (&tfm[numtfms]);
  if (tfm_verbose) {
    dump_sizes (&tfm[numtfms]);
  }
  numtfms += 1;
}









