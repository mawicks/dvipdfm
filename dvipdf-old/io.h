#include <stdio.h>
#include "numbers.h"

UNSIGNED_BYTE read_byte (FILE *);

void seek_absolute (FILE *file, long pos);
void seek_relative (FILE *file, long pos);

void seek_end (FILE *file);

long tell_position (FILE *file);

long file_size (FILE *file);

extern char work_buffer[];
#define WORK_BUFFER_SIZE 1024
