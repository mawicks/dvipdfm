#ifndef NUMBERS_H
#define NUMBERS_H

#include <stdio.h>

typedef int UNSIGNED_BYTE, SIGNED_BYTE, SIGNED_PAIR;
typedef unsigned UNSIGNED_PAIR;
typedef long  UNSIGNED_TRIPLE, SIGNED_TRIPLE, SIGNED_QUAD;
typedef unsigned long UNSIGNED_QUAD;

UNSIGNED_BYTE get_unsigned_byte (FILE *);

SIGNED_BYTE get_signed_byte (FILE *);

UNSIGNED_PAIR get_unsigned_pair (FILE *);

SIGNED_PAIR get_signed_pair (FILE *);

UNSIGNED_TRIPLE get_unsigned_triple (FILE *);

SIGNED_TRIPLE get_signed_triple (FILE *);

SIGNED_QUAD get_signed_quad (FILE *);

UNSIGNED_QUAD get_unsigned_quad (FILE *);

#define ROUND(n,acc) (floor((n)/(acc)+0.5)*(acc)) 

#endif /* NUMBERS_H */

