#include <stdio.h>
#include <string.h>
#include "numbers.h"
#include "mem.h"
#include "mfileio.h"

#define fixed(a) ((double)((a)&0xffff)/(double)(1<<16)+ \
 (((a)>>16) > 0x8000? 0x10000l - ((a)>>16): ((a)>>16)))

struct table_directory
{
  SIGNED_QUAD version;
  UNSIGNED_PAIR num_tables, search_range,
    entry_selector, range_shift;
} table_directory;

struct table_header
{ 
  char tag[5];
  UNSIGNED_QUAD check_sum, offset, length;
} *table;

FILE *ttf_file;


struct post_header
{
  UNSIGNED_QUAD format;
  UNSIGNED_QUAD italicAngle;
  SIGNED_PAIR underlinePosition;
  SIGNED_PAIR underlineThickness;
  UNSIGNED_QUAD isFixedPitch;
} post_header;

void read_post_table(void)
{
  fprintf (stdout, "Table type: post\n");
  post_header.format = get_unsigned_quad(ttf_file);
  fprintf (stdout, "\tVersion: %f\n", fixed(post_header.format));
  post_header.italicAngle = get_unsigned_quad (ttf_file);
  fprintf (stdout, "\tItalic Angle: %f\n",
	   fixed(post_header.italicAngle));
  post_header.underlinePosition = get_signed_pair (ttf_file);
  fprintf (stdout, "\tUnderline Position: %d\n",
	   post_header.underlinePosition);
  post_header.underlineThickness = get_signed_pair (ttf_file);
  fprintf (stdout, "\tUnderline Thickness: %d\n",
	   post_header.underlineThickness);
  post_header.isFixedPitch = get_unsigned_quad (ttf_file);
  fprintf (stdout, "\tIs Fixed Pitch?: %ld\n",
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
} horz_header;

void read_hhea_table (void)
{
  fprintf (stdout, "Table type: hhea\n");
  horz_header.version = get_unsigned_quad(ttf_file);
  fprintf (stdout, "\tVersion: %f\n", fixed(horz_header.version));
  horz_header.ascender = get_signed_pair (ttf_file);
  fprintf (stdout, "\tAscender: %d\n", horz_header.ascender);
  horz_header.descender = get_signed_pair (ttf_file);
  fprintf (stdout, "\tDescender: %d\n", horz_header.descender);
  horz_header.line_gap = get_signed_pair (ttf_file);
  fprintf (stdout, "\tLine Gap: %d\n", horz_header.line_gap);
}


struct font_header 
{
  UNSIGNED_QUAD version, revision;
  UNSIGNED_QUAD check_sum;
  UNSIGNED_QUAD magic;
  UNSIGNED_PAIR flags, units_per_em;
  SIGNED_PAIR xMin, yMin, xMax, yMax;
  UNSIGNED_PAIR style, minsize;
  SIGNED_PAIR fontDirectionHint, indexToLocFormat;
  SIGNED_PAIR glyphDataFormat;
} font_header;

void read_head_table(void)
{
  int i;
  fprintf (stdout, "Table type: head\n");
  font_header.version = get_unsigned_quad(ttf_file);
  fprintf (stdout, "\tVersion: %f\n", fixed(font_header.version));
  font_header.revision = get_unsigned_quad(ttf_file);
  fprintf (stdout, "\tRevision: %f\n", fixed(font_header.revision));
  font_header.check_sum = get_unsigned_quad(ttf_file);
  fprintf (stdout, "\tChecksum: %lx\n", font_header.check_sum);
  font_header.magic = get_unsigned_quad(ttf_file);
  fprintf (stdout, "\tMagic: %lx\n", font_header.magic);
  font_header.flags = get_unsigned_pair(ttf_file);
  fprintf (stdout, "\tFlags: %x\n", font_header.flags);
  font_header.units_per_em = get_unsigned_pair(ttf_file);
  fprintf (stdout, "\tunits_per_em: %d\n", font_header.units_per_em);
  /* Skip Created Date*/
  for (i=0; i<8; i++) {
    fgetc (ttf_file);
  }
  /* Skip Modified Date*/
  for (i=0; i<8; i++) {
    fgetc (ttf_file);
  }
  font_header.xMin = get_signed_pair(ttf_file);
  fprintf (stdout, "\txMin: %d\n", font_header.xMin);
  font_header.yMin = get_signed_pair(ttf_file);
  fprintf (stdout, "\tyMin: %d\n", font_header.yMin);
  font_header.xMax = get_signed_pair(ttf_file);
  fprintf (stdout, "\txMax: %d\n", font_header.xMax);
  font_header.yMax = get_signed_pair(ttf_file);
  fprintf (stdout, "\tyMax: %d\n", font_header.yMax);
  font_header.style = get_unsigned_pair(ttf_file);
  fprintf (stdout, "\tyStyle: %d\n", font_header.style);
  font_header.minsize = get_unsigned_pair(ttf_file);
  fprintf (stdout, "\tyMin readable size (pixels): %d\n", font_header.minsize);
  font_header.fontDirectionHint = get_signed_pair(ttf_file);
  fprintf (stdout, "\tDirection Hint: %d\n", font_header.fontDirectionHint);
  font_header.indexToLocFormat = get_signed_pair(ttf_file);
  fprintf (stdout, "\tIndex Format: %d\n",
	   font_header.indexToLocFormat);
  font_header.glyphDataFormat = get_signed_pair(ttf_file);
  fprintf (stdout, "\tData Format: %d\n",
	   font_header.glyphDataFormat);
}

void convert_tag (char *tag, unsigned long u_tag)
{
  int i;
  for (i=3; i>= 0; i--) {
    tag[i] = u_tag & 0xff;
    u_tag >>= 8;
  }
  tag[4] = 0;
}


void read_directory()
{
  unsigned long i;
  table_directory.version = get_unsigned_quad(ttf_file);
  table_directory.num_tables = get_unsigned_pair (ttf_file);
  table_directory.search_range = get_unsigned_pair (ttf_file);
  table_directory.entry_selector = get_unsigned_pair (ttf_file);
  table_directory.range_shift = get_unsigned_pair (ttf_file);
  fprintf (stdout, "File Header\n");
  fprintf (stdout, "\tVersion: %.5f\n", fixed(table_directory.version));
  fprintf (stdout, "\tNumber of tables: %d\n",
	   table_directory.num_tables);
  fprintf (stdout, "\tSearch Range: %d\n", table_directory.search_range);
  fprintf (stdout, "\tEntry Selector: %d\n",
	   table_directory.entry_selector);
  fprintf (stdout, "\tRange Shift: %d\n",
	   table_directory.range_shift);
  table = NEW (table_directory.num_tables, struct table_header);
  for (i=0; i<table_directory.num_tables; i++) {
    unsigned long u_tag;
    fprintf (stdout, "New Table\n");
    u_tag = get_unsigned_quad (ttf_file);
    convert_tag (table[i].tag, u_tag);
    fprintf (stdout, "\tTag: %4s\n", table[i].tag);
    table[i].check_sum = get_unsigned_quad (ttf_file);
    fprintf (stdout, "\tChecksum: %lx\n", table[i].check_sum);
    table[i].offset = get_unsigned_quad (ttf_file);
    fprintf (stdout, "\tOffset: %lx\n", table[i].offset);
    table[i].length = get_unsigned_quad (ttf_file);
    fprintf (stdout, "\tLength: %lx\n", table[i].length);
  }
  fprintf (stdout, "**End of directory**\n");
}


int main (int argc, char *argv[]) 
{
  int i;
  if (argc <= 1) {
    fprintf (stderr, "Usage: %s ttf_file_name\n", argv[0]);
  }
  if ((ttf_file = FOPEN (argv[1], "rb")) == NULL) {
    fprintf (stderr, "Unable to open %s\n", argv[1]);
  }
  read_directory();
  for (i=0; i<table_directory.num_tables; i++) {
    seek_absolute (ttf_file, table[i].offset);
    if (strncmp (table[i].tag, "head", 4) == 0) {
      read_head_table(); 
    } else if  (strncmp (table[i].tag, "hhea", 4) == 0) {
      read_hhea_table(); 
    } else if  (strncmp (table[i].tag, "post", 4) == 0) {
      read_post_table(); 
    } else {
      fprintf (stdout, "Skipping table %4s\n", table[i].tag);
    }
  }
  return 0;
}

