#include <stdio.h>
#include "mem.h"

void *new (unsigned long size)
{
  void *result;
  result = malloc (size);
  if (!result) {
    fprintf (stderr, "Out of memory!\n");
    exit (1);
  }
  return result;
}

void release (void *mem)
{
  free (mem);
}

