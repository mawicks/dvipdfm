#define U(a) ((unsigned char)(a))
#define twiddle(n) (( \
     (U(n)<=32) || \
     (U(n)>=128u&&U(n)<=160u) || \
     (U(n) == 127) || (U(n) == 255u))? (U(n)^128u):(n))

