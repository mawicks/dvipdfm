#include <stdio.h>
#include <ctype.h>

static int nextch(void)
{
  int ch;
  while ((ch = getchar()) >= 0) {
    if (ch >= '!' && ch <= 'u' || ch == 'z')
      return ch;
    if (ch == '~' && getchar() == '>') {
      return -1;
    }
  }
  if (ch < 0) {
    fprintf (stderr, "Premature end of file.\n");
    exit(1);
  }
}

unsigned long int ascii85;
unsigned binary[4];

static decode_block(void)
{
  int i;
  for (i=3; i>=0; i--){
    binary[i] = ascii85 % 256u;
    ascii85 /= 256u;
  }
}

static output_block (int n)
{
  int i;
  for (i=0; i<n; i++) {
    putchar (binary[i]);
  }
}

main (int argc, char *argv[])
{
  int i, ch, eof = 0, nread;
  while (!eof) {
    ascii85 = 0;
    nread = 0;
    /* Look ahead for special zero key */
    if ((ch = nextch()) == 'z') {
      nread = 4;  /* Lie to it */
    } else{
      ungetc (ch, stdin);
      for (i=0; i<5; i++) {
	if ((ch=nextch()) < 0) {
	  eof = 1;
	  break;
	}
	ascii85 = ascii85 * 85 + (ch-'!');
	nread += 1;
      }
    }
    if (nread > 1) {
      decode_block();
      output_block(nread-1);
    }
  }
}
