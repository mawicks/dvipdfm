#ifndef ERROR_H
  #define FATAL_ERROR -1

  #define ERROR(string) {fprintf (stderr, string);fprintf (stderr, "\n"); exit(1);}
  typedef int error_t;

#else
  #define ERROR_H
#endif 

