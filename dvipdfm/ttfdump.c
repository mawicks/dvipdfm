#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "system.h"
#include "mem.h"
#include "error.h"
#include "mfileio.h"
#include "pdfobj.h"
#include "numbers.h"
#include "standardenc.h"

#define verbose 4

/* Convert ttf "fixed" type to double */

#define fixed(a) ((double)((a)%0x10000L)/(double)(0x10000L)+ \
 (((a)/0x10000L) > 0x8000L? 0x10000L - ((a)/0x10000L): ((a)/0x10000L)))

/* Convert four-byte number to big endianess in a machine independent
   way */
static void convert_tag (char *tag, unsigned long u_tag)
{
  int i;
  for (i=3; i>= 0; i--) {
    tag[i] = u_tag % 256;
    u_tag /= 256;
  }
  tag[4] = 0;
}

struct table_header
{ 
  char tag[5];
  UNSIGNED_QUAD check_sum, offset, length;
  int omit;	/* If an application call sets omit=1, this table will
		   not be written to the output by ttf_build_font() */
  void *table_data;
};

struct table_directory
{
  SIGNED_QUAD version;
  UNSIGNED_PAIR num_tables, search_range,
    entry_selector, range_shift;
  struct table_header *tables;
};

static void release_directory (struct table_directory *r) 
{
  if (r && r->tables) {
    RELEASE (r->tables);
  }
  if (r)
    RELEASE (r);
}

static int put_big_endian (char *s, UNSIGNED_QUAD q, int n)
{
  int i;
  for (i=n-1; i>=0; i--) {
    s[i] = (char) q%256;
    q/=256L;
  }
  return n;
}

static unsigned max2floor(unsigned n)
/* Computes the max power of 2 <= n */
{
  int i = 1;
  while (n > 1) {
    n /= 2;
    i *= 2;
  }
  return i;
}

static unsigned log2floor(unsigned  n)
/* Computes the log2 of the max power of 2 <= n */
{
  unsigned i = 0;
  while (n > 1) {
    n /= 2;
    i += 1;
  }
  return i;
}


static char *ttf_build_font (FILE *ttf_file, struct table_directory
			     *td, long *size) 
{
  char *result, *p;
  long font_size = 0, offset = 0;
  int i, num_kept_tables = 0, new_search_range;
  for (i=0; i<(td->num_tables); i++) {
    if (!(td->tables)[i].omit) {
      font_size += (td->tables[i].length);
      num_kept_tables += 1;
    }
  }
  font_size += (td->num_tables)*16; /* 16 bytes per table entry */
  font_size += 12; /* 12 bytes for the directory */
  *size = font_size;
  result = NEW (font_size, char);
  p = result;
  { /* Header */
    p += put_big_endian (p, td->version, 4);
    p += put_big_endian (p, num_kept_tables, 2);
    new_search_range = max2floor(num_kept_tables) * 16;
    p += put_big_endian (p, new_search_range, 2);
    p += put_big_endian (p, log2floor(num_kept_tables), 2);
    p += put_big_endian (p, num_kept_tables*16-new_search_range, 2);
  }
  /* Computer start of actual tables (after headers) */
  offset = 12 + 16 * num_kept_tables;
  for (i=0; i<(td->num_tables); i++) {
    if (!(td->tables)[i].omit) {
      p += sprintf (p, "%4s", (td->tables)[i].tag);
      p += put_big_endian (p, (td->tables)[i].check_sum, 4);
      p += put_big_endian (p, offset, 4);
      p += put_big_endian (p, (td->tables)[i].length, 4);
      /* Be careful here.  Offset to the right place in the file
	 using the old offset and read the data into the right
	 place in the buffer using the new offset */
      seek_absolute (ttf_file, (td->tables)[i].offset);
      fread (result+offset, (td->tables)[i].length, sizeof(char), ttf_file);
      /* Set offset for next table */
      offset += (td->tables)[i].length;
      if (offset > font_size )
	ERROR ("Uh oh");
    }
  }
  return result;
}


static struct table_directory *read_directory(FILE *ttf_file)
{
  unsigned long i;
  struct table_directory *td = NEW (1, struct table_directory);
  rewind (ttf_file);
  td -> version = get_unsigned_quad(ttf_file);
  td -> num_tables = get_unsigned_pair (ttf_file);
  td -> search_range = get_unsigned_pair (ttf_file);
  td -> entry_selector = get_unsigned_pair (ttf_file);
  td -> range_shift = get_unsigned_pair (ttf_file);
  fprintf (stderr, "File Header\n");
  fprintf (stderr, "\tVersion: %.5f\n", fixed(td -> version));
  fprintf (stderr, "\tNumber of tables: %d\n",
	   td -> num_tables);
  fprintf (stderr, "\tSearch Range: %d\n", td -> search_range);
  fprintf (stderr, "\tEntry Selector: %d\n",
	   td -> entry_selector);
  fprintf (stderr, "\tRange Shift: %d\n",
	   td -> range_shift);
  td->tables = NEW (td -> num_tables, struct table_header);
  for (i=0; i < td->num_tables; i++) {
    unsigned long u_tag;
    fprintf (stderr, "New Table\n");
    u_tag = get_unsigned_quad (ttf_file);
    convert_tag ((td->tables)[i].tag, u_tag);
    fprintf (stderr, "\tTag: \"%4s\"\n", (td->tables)[i].tag);
    (td->tables)[i].check_sum = get_unsigned_quad (ttf_file);
    fprintf (stderr, "\tChecksum: %lx\n", (td->tables)[i].check_sum);
    (td->tables)[i].offset = get_unsigned_quad (ttf_file);
    fprintf (stderr, "\tOffset: %lx\n", (td->tables)[i].offset);
    (td->tables)[i].length = get_unsigned_quad (ttf_file);
    fprintf (stderr, "\tLength: %lx\n", (td->tables)[i].length);
    (td->tables)[i].omit = 0;
    (td->tables)[i].table_data = NULL;
  }
  fprintf (stderr, "**End of directory**\n");
  return td;
}

static int find_table_index (struct table_directory *td, char *tag)
{
  int result, i;
  for (i=0; i < td->num_tables; i++) {
    if (!strncmp ((td->tables)[i].tag, tag, 4)) {
      break;
    }
  }
  if (i < td-> num_tables)
    result = i;
  else 
    result = -1;
  return result;
}


static long find_table_pos (struct table_directory *td, char *tag) 
{
  int i;
  long result = -1;
  if ((i=find_table_index (td, tag)) >= 0) {
    result = (td->tables)[i].offset;
  }
  return result;
}

static long find_table_length (struct table_directory *td, char *tag) 
{
  int i;
  long result = -1;
  if ((i=find_table_index (td, tag)) >= 0) {
    result = (td->tables)[i].length;
  }
  return result;
}

static struct post_header
{
  UNSIGNED_QUAD format;
  UNSIGNED_QUAD italicAngle;
  SIGNED_PAIR underlinePosition;
  SIGNED_PAIR underlineThickness;
  UNSIGNED_QUAD isFixedPitch;
} post_header;

static void read_post_table(FILE *ttf_file, struct table_directory *td)
{
  long post_offset = find_table_pos (td, "post");
  seek_absolute (ttf_file, post_offset);
  fprintf (stderr, "post table @ %ld\n", post_offset);
  fprintf (stderr, "Table type: post\n");
  post_header.format = get_unsigned_quad(ttf_file);
  fprintf (stderr, "version: %lx\n", post_header.format);
  fprintf (stderr, "\tVersion: %f\n", fixed(post_header.format));
  post_header.italicAngle = get_unsigned_quad (ttf_file);
  fprintf (stderr, "\tItalic Angle: %f\n",
	   fixed(post_header.italicAngle));
  post_header.underlinePosition = get_signed_pair (ttf_file);
  fprintf (stderr, "\tUnderline Position: %d\n",
	   post_header.underlinePosition);
  post_header.underlineThickness = get_signed_pair (ttf_file);
  fprintf (stderr, "\tUnderline Thickness: %d\n",
	   post_header.underlineThickness);
  post_header.isFixedPitch = get_unsigned_quad (ttf_file);
  fprintf (stderr, "\tIs Fixed Pitch?: %ld\n",
	   post_header.isFixedPitch);
}


struct horz_header
{
  UNSIGNED_QUAD version;
  SIGNED_PAIR ascender, descender, line_gap;
  UNSIGNED_PAIR advanceWidthMax;
  SIGNED_PAIR minLeftSideBearing, minRightSideBearing, xMaxExtent;
  SIGNED_PAIR caretSlopeRise, caretSlopeRun;
  UNSIGNED_PAIR numberOfHMetrics;
};

void read_hhea_table (FILE *ttf_file, struct table_directory *td)
{
  struct horz_header hh;
  long hhea_offset = find_table_pos (td, "hhea");
  seek_absolute (ttf_file, hhea_offset);
  fprintf (stderr, "hhea table @ %ld\n", hhea_offset);
  fprintf (stderr, "Table type: hhea\n");
  hh.version = get_unsigned_quad(ttf_file);
  fprintf (stderr, "\tVersion: %f\n", fixed(hh.version));
  hh.ascender = get_signed_pair (ttf_file);
  fprintf (stderr, "\tAscender: %d\n", hh.ascender);
  hh.descender = get_signed_pair (ttf_file);
  fprintf (stderr, "\tDescender: %d\n", hh.descender);
  hh.line_gap = get_signed_pair (ttf_file);
  fprintf (stderr, "\tLine Gap: %d\n", hh.line_gap);
}

struct font_header 
{
  double version, revision;
  UNSIGNED_QUAD check_sum;
  UNSIGNED_QUAD magic;
  UNSIGNED_PAIR flags, units_per_em;
  SIGNED_PAIR xMin, yMin, xMax, yMax;
  UNSIGNED_PAIR style, minsize;
  SIGNED_PAIR fontDirectionHint, indexToLocFormat;
  SIGNED_PAIR glyphDataFormat;
};

struct font_header *read_head_table(FILE *ttf_file, struct table_directory *td)
{
  struct font_header *r;
  int i;
  unsigned long fw;
  long head_offset = find_table_pos (td, "head");
  seek_absolute (ttf_file, head_offset);
  r = NEW (1, struct font_header);
  fprintf (stderr, "Table type: head\n");
  fw = get_unsigned_quad(ttf_file);
  (r->version) = fixed(fw);
  fprintf (stderr, "\tVersion: %f\n", r->version);
  fw = get_unsigned_quad(ttf_file);
  (r->revision) = fixed(fw);
  fprintf (stderr, "\tRevision: %f\n", r->revision);
  r->check_sum = get_unsigned_quad(ttf_file);
  fprintf (stderr, "\tChecksum: %lx\n", r->check_sum);
  r->magic = get_unsigned_quad(ttf_file);
  fprintf (stderr, "\tMagic: %lx\n", r->magic);
  r->flags = get_unsigned_pair(ttf_file);
  fprintf (stderr, "\tFlags: %x\n", r->flags);
  r->units_per_em = get_unsigned_pair(ttf_file);
  fprintf (stderr, "\tunits_per_em: %d\n", r->units_per_em);
  /* Skip Dates */
  for (i=0; i<4; i++) {
    get_unsigned_quad (ttf_file);
  }
  r->xMin = get_signed_pair(ttf_file);
  fprintf (stderr, "\txMin: %d\n", r->xMin);
  r->yMin = get_signed_pair(ttf_file);
  fprintf (stderr, "\tyMin: %d\n", r->yMin);
  r->xMax = get_signed_pair(ttf_file);
  fprintf (stderr, "\txMax: %d\n", r->xMax);
  r->yMax = get_signed_pair(ttf_file);
  fprintf (stderr, "\tyMax: %d\n", r->yMax);
  r->style = get_unsigned_pair(ttf_file);
  fprintf (stderr, "\tyStyle: %d\n", r->style);
  r->minsize = get_unsigned_pair(ttf_file);
  fprintf (stderr, "\tyMin readable size (pixels): %d\n", r->minsize);
  r->fontDirectionHint = get_signed_pair(ttf_file);
  fprintf (stderr, "\tDirection Hint: %d\n", r->fontDirectionHint);
  r->indexToLocFormat = get_signed_pair(ttf_file);
  fprintf (stderr, "\tIndex Format: %d\n",
	   r->indexToLocFormat);
  r->glyphDataFormat = get_signed_pair(ttf_file);
  fprintf (stderr, "\tData Format: %d\n",
	   r->glyphDataFormat);
  return r;
}

struct maxp_table 
{
  long version;
  unsigned numGlyphs, maxPoints, maxContours,  maxCompositePoints;
  unsigned maxCompositeContours, maxZones, maxTwilightPoints;
  unsigned maxStorage, maxFunctionDefs, maxInstructionDefs;
  unsigned maxStackElements, maxSizeOfInstructions;
  unsigned maxComponentElements, maxComponentDepth;
};

struct maxp_table *read_maxp_table (FILE *ttf_file, struct table_directory *td)
{
  struct maxp_table *r;
  long maxp_offset = find_table_pos (td, "maxp");
  fprintf (stderr, "read_maxp_table()\n");
  seek_absolute (ttf_file, maxp_offset);
  r = NEW (1, struct maxp_table);
  r->version = get_unsigned_quad (ttf_file);
  r->numGlyphs = get_unsigned_pair (ttf_file);
  r->maxPoints = get_unsigned_pair (ttf_file);
  r->maxContours = get_unsigned_pair (ttf_file);
  r->maxCompositePoints = get_unsigned_pair (ttf_file);
  r->maxCompositeContours = get_unsigned_pair (ttf_file);
  r->maxZones = get_unsigned_pair (ttf_file);
  r->maxTwilightPoints = get_unsigned_pair (ttf_file);
  r->maxStorage = get_unsigned_pair (ttf_file);
  r->maxFunctionDefs = get_unsigned_pair (ttf_file);
  r->maxInstructionDefs = get_unsigned_pair (ttf_file);
  r->maxStackElements = get_unsigned_pair (ttf_file);
  r->maxSizeOfInstructions = get_unsigned_pair (ttf_file);
  r->maxComponentElements = get_unsigned_pair (ttf_file);
  r->maxComponentDepth = get_unsigned_pair (ttf_file);
  return r;
}

unsigned long *read_loc_table (FILE *ttf_file, struct
				  table_directory *td, int numGlyphs)
{
  unsigned i;
  struct font_header *r;
  unsigned long *loc_table = NEW (numGlyphs+1, unsigned long);
  long loca_offset;

  fprintf (stderr, "read_loc_table()\n");

  r = read_head_table (ttf_file, td);

  loca_offset = find_table_pos (td, "loca");
  seek_absolute (ttf_file, loca_offset);

  fprintf (stderr, "loca table @ %ld(%lx)\n", loca_offset,
	   loca_offset);

  switch (r->indexToLocFormat) {
  case 0:
    fprintf (stderr, "Short format loc table\n");
    for (i=0; i<=numGlyphs; i++) {
      loc_table[i] = 2L*get_unsigned_pair (ttf_file);
      fprintf (stderr, "loc[%d]=%ld\n", i, loc_table[i]);
    }
    break;
  case 1:
    fprintf (stderr, "Long format loc table\n");
    fprintf (stderr, "Short format loc table\n");
    for (i=0; i<=numGlyphs; i++) {
      loc_table[i] = get_unsigned_quad (ttf_file);
      fprintf (stderr, "loc[%d]=%ld\n", i, loc_table[i]);
    }
    break;
  }
  return loc_table;
}



char **read_glyph_names (FILE *ttf_file, struct
			table_directory *td, int numGlyphs)
{
  char *pool, *s, **poolstrings;
  int poolsize = 0;
  unsigned i;
  unsigned long format, nleft;
  char **glyph_names = NEW (numGlyphs, char *);
  unsigned *name_indices = NEW (numGlyphs, unsigned);
  long post_offset;
  post_offset = find_table_pos (td, "post");
  fprintf (stderr, "read_glyph_names()\n");
  seek_absolute (ttf_file, post_offset);
  format = get_unsigned_quad (ttf_file);
  seek_absolute (ttf_file, post_offset+32);
  nleft -= 32;
  fprintf (stderr, "format = 0x%lx\n", format);
  switch (format) {
  case 0x20000: /* format 2 */
    if (numGlyphs != get_unsigned_pair (ttf_file))
      ERROR ("numGlyphs mismatch in post table");
    for (i=0; i<numGlyphs; i++) {
      name_indices[i] = get_unsigned_pair (ttf_file);
      fprintf (stderr, "name_index[%d] = %d\n", i, name_indices[i]);
    }
    nleft = find_table_length (td, "post") - 34 - 2*numGlyphs;
    fprintf (stderr, "nleft=%ld\n", nleft);
    /* Read the pool of strings at the end */
    pool = NEW (nleft, char);
    fread (pool, nleft, sizeof(char), ttf_file);
    fprintf (stderr, ">>%s<<", pool);

    /* First pass:  Count the strings in the pool */
    for (s=pool; s<pool+nleft; ) {
      int len;
      len = sget_unsigned_byte (s);
      s += len+1;
      poolsize += 1;
    } 
    if (s != pool+nleft){
      fprintf (stderr, "\n\nPossibly out of sync in post name pool table\n\n");
    }
    
    poolstrings = NEW (poolsize, char *);
    /* Second pass: copy the pool strings to an array */
    for (s=pool, i=0; i<poolsize; i++) {
      int len;
      len = sget_unsigned_byte (s);
      poolstrings[i] = NEW (len+1, char);
      strncpy (poolstrings[i], s+1, len);
      poolstrings[i][len] = 0;
      fprintf (stderr, "poolstrings[%d]=%s\n", i, poolstrings[i]);
      s += len+1;
    }
    
    for (i=0; i<numGlyphs; i++) {
      if (name_indices[i] < 258) {
	glyph_names[i] = NEW (strlen(macglyphorder[i])+1, char);
	strcpy (glyph_names[i], macglyphorder[i]);
      } else if (name_indices[i] >= 258) {
	glyph_names[i] = NEW (strlen(poolstrings[name_indices[i]-258])+1, char);
	strcpy (glyph_names[i], poolstrings[name_indices[i]-258]);
      }
      fprintf (stderr, "name[%d]=%s\n", i, glyph_names[i]);
    }
    break;
  }
  return glyph_names;
}


struct char_info 
{
  unsigned long *loca;
  char **names;
  char *glyph_data;
  int numGlyphs;
};


struct char_info *get_char_info (FILE *ttf_file,
				 struct table_directory *td) 
{
  struct maxp_table *mp;
  struct char_info *ci;
  /* First determine number of glyphs */
  mp = read_maxp_table (ttf_file, td);
  ci = NEW (1, struct char_info);
  ci->numGlyphs = mp->numGlyphs;
  if (verbose > 3)
    fprintf (stderr, "numGlyphs = %d\n", ci->numGlyphs);
  RELEASE (mp);
  /* Now get the locations */
  ci->loca = read_loc_table (ttf_file, td, ci->numGlyphs);
  {
    int i;
    for (i=0; i<=ci->numGlyphs; i++) {
      fprintf (stderr, "loca[%d]=%ld\n", i, (ci->loca)[i]);
    }
  }
  /* Now get the glyph names */
  ci->names = read_glyph_names (ttf_file, td, ci->numGlyphs);
  return ci;
}
				 

void dump_glyphs (FILE *ttf_file, struct table_directory *td)
{
  long post_offset = find_table_pos (td, "post");
  seek_absolute (ttf_file, post_offset);
  fprintf (stderr, "post table @ %ld\n", post_offset);
  get_char_info (ttf_file, td);
}


void error_cleanup(void)
{
  return;
}

int main (int argc, char *argv[]) 
{
  FILE *ttf_file;
  struct table_directory *td;
  if (argc <= 1) {
    fprintf (stderr, "Usage: %s ttf_file_name\n", argv[0]);
  }
  if ((ttf_file = MFOPEN (argv[1], "rb")) == NULL) {
    fprintf (stderr, "Unable to open %s\n", argv[1]);
  }
  
  td = read_directory(ttf_file);
  dump_glyphs(ttf_file, td);
  return 0;
  read_head_table (ttf_file, td);
  read_hhea_table (ttf_file, td);
  read_post_table (ttf_file, td);
  return 0;
}

