pdfdev.o: pdfdev.c pdfdev.h numbers.h pdfdoc.h pdfobj.h error.h \
 system.h type1.h mem.h mfileio.h pdfspecial.h pdflimits.h tfm.h dvi.h \
 vf.h
pdfdoc.o: pdfdoc.c c-auto.h system.h pdflimits.h pdfobj.h error.h \
 mem.h pdfdoc.h pdfdev.h numbers.h mfileio.h
dvi.o: dvi.c error.h system.h numbers.h mfileio.h pdflimits.h pdfdev.h \
 tfm.h mem.h dvi.h
mfileio.o: mfileio.c mfileio.h numbers.h error.h system.h
mem.o: mem.c mem.h
jpeg.o: jpeg.c system.h mfileio.h numbers.h mem.h jpeg.h pdfobj.h \
 pdfspecial.h dvi.h error.h pdfdev.h
numbers.o: numbers.c numbers.h error.h system.h mfileio.h
pdfobj.o: pdfobj.c pdflimits.h system.h pdfobj.h mem.h error.h \
 mfileio.h numbers.h pdfspecial.h pdfparse.h
pdfparse.o: pdfparse.c system.h pdfparse.h numbers.h pdfobj.h \
 pdfspecial.h pdfdoc.h pdfdev.h mem.h dvi.h error.h mfileio.h
pdfspecial.o: pdfspecial.c system.h pdflimits.h pdfspecial.h numbers.h \
 pdfobj.h pdfdoc.h pdfdev.h pdfparse.h mem.h dvi.h error.h mfileio.h \
 jpeg.h epdf.h
tfm.o: tfm.c system.h pdflimits.h numbers.h error.h mfileio.h mem.h
type1.o: type1.c system.h pdfobj.h mem.h error.h mfileio.h numbers.h \
 type1.h tfm.h pdfparse.h pdflimits.h winansi.h
dvipdfm.o: dvipdfm.c c-auto.h dvi.h error.h system.h numbers.h \
 pdfdev.h mem.h pdfdoc.h pdfobj.h type1.h
epdf.o: epdf.c system.h pdfobj.h pdfdoc.h pdfspecial.h numbers.h \
 mfileio.h epdf.h mem.h
vf.o: vf.c pdflimits.h numbers.h mem.h error.h system.h tfm.h pdfdev.h \
 dvi.h dvicodes.h
