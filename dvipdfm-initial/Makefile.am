include_HEADERS = dvi.h error.h io.h tfm.h numbers.h pdfobj.h pdfdoc.h type1.h pdfdev.h pdfspecial.h pdfparse.h jpeg.h
bin_PROGRAMS = dvipdf dvitest 
dvitest_SOURCES = dvitest.c numbers.c dvi.c io.c mem.c pdfdev.c pdfdoc.c pdfobj.c type1.c tfm.c pdfspecial.c pdfparse.c jpeg.c
dvipdf_LDADD = ./kpathsea/kpathsea/STATIC/libkpathsea.a
dvipdf_SOURCES = dvipdf.c numbers.c dvi.c io.c mem.c pdfdev.c pdfdoc.c pdfobj.c type1.c tfm.c pdfspecial.c pdfparse.c jpeg.c
dvitest_LDADD = ./kpathsea/kpathsea/STATIC/libkpathsea.a
