#ifndef _SYSTEM_H_

#define _SYSTEM_H_

#include "c-auto.h"

#ifdef KPATHSEA
#include <kpathsea/config.h>
#include <kpathsea/c-fopen.h>
#include <kpathsea/c-memstr.h>
#include <kpathsea/tex-file.h>
	
#ifdef __STDC__
#include "pdfobj.h"
extern void pdf_doc_creator(char *s);
extern void pdf_set_info(pdf_obj *object);
extern void pdf_set_root(pdf_obj *object);
extern void dev_do_special(char *buffer,unsigned long size);
extern void pdf_doc_add_to_page_xobjects(const char *name,pdf_obj *resource);
extern void pdf_doc_add_to_page(char *buffer,unsigned int length);
static void release_stream(pdf_stream *stream);
extern struct pdf_obj *parse_pdf_null(char **start ,char *end);
extern void dvi_do_page(int n);
extern int dvi_npages(void );
extern void *renew(void *mem,unsigned long size);
#endif /* __STDC__ */

#else

#define FOPEN_R_MODE "rb"
#define FOPEN_RBIN_MODE "rb"
#define FOPEN_WBIN_MODE "wb"
#endif /* KPATHSEA */

#ifdef WIN32
#  undef ERROR
#  undef NO_ERROR
#  undef OUT
#  undef CDECL
#  define CDECL __cdecl
#else
#  define CDECL
#endif /* WIN32 */

#endif /* _SYSTEM_H_ */
