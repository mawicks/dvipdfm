#include "io.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

static void os_error()
{
  ERROR ("io:  An OS command failed that should not have.\n");
}

UNSIGNED_BYTE read_byte (FILE *file) 
{
  int byte;
  if (feof(file)) {
    ERROR ("File ended prematurely");
  }
  if ((byte = fgetc (file)) < 0) {
    ERROR ("read_byte:  OS error");
  }
  return (UNSIGNED_BYTE) byte;
}

void seek_absolute (FILE *file, long pos) 
{
  if (fseek(file, pos, SEEK_SET)) {
    os_error();
  }
}

void seek_relative (FILE *file, long pos)
{
  if (fseek(file, pos, SEEK_CUR)) {
    os_error();
  }
}


void seek_end (FILE *file) 
{
  if (fseek(file, 0L, SEEK_END)) {
    os_error();
  }
}

long tell_position (FILE *file) 
{
  long size;
  if ((size = ftell (file)) < 0) {
    os_error();
  }
}

long file_size (FILE *file)
{
  long size;
  /* Seek to end */
  seek_end (file);

  size = tell_position (file);
  rewind (file);
  return (size);
}



