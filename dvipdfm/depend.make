pdfdev.o: pdfdev.c pdfdev.h numbers.h pdfdoc.h pdfobj.h error.h \
 type1.h pkfont.h mem.h mfileio.h pdfspecial.h pdfparse.h tpic.h \
 htex.h psspecial.h pdflimits.h tfm.h dvi.h colors.h
pdfdoc.o: pdfdoc.c config.h system.h pdflimits.h pdfobj.h error.h \
 mem.h dvi.h numbers.h pdfdev.h pdfdoc.h pdfspecial.h mfileio.h \
 thumbnail.h
dvi.o: dvi.c system.h error.h numbers.h mfileio.h pdflimits.h pdfdev.h \
 pdfdoc.h pdfobj.h tfm.h mem.h dvi.h vf.h dvicodes.h
mfileio.o: mfileio.c mfileio.h numbers.h error.h
mem.o: mem.c mem.h
jpeg.o: jpeg.c system.h mfileio.h numbers.h mem.h jpeg.h pdfobj.h \
 pdfspecial.h dvi.h error.h pdfdev.h
numbers.o: numbers.c numbers.h error.h mfileio.h
pdfobj.o: pdfobj.c system.h pdflimits.h config.h pdfobj.h mem.h \
 error.h mfileio.h numbers.h pdfspecial.h pdfparse.h twiddle.h
pdfparse.o: pdfparse.c system.h pdfparse.h numbers.h pdfobj.h \
 pdfspecial.h pdfdoc.h pdfdev.h mem.h dvi.h error.h mfileio.h
pdfspecial.o: pdfspecial.c system.h pdflimits.h pdfspecial.h numbers.h \
 pdfobj.h pdfdoc.h pdfdev.h pdfparse.h mem.h dvi.h error.h mfileio.h \
 jpeg.h epdf.h mpost.h psimage.h config.h pngimage.h
tfm.o: tfm.c system.h pdflimits.h numbers.h error.h mfileio.h mem.h
type1.o: type1.c system.h pdfobj.h mem.h error.h mfileio.h numbers.h \
 type1.h tfm.h pdfparse.h pdflimits.h t1crypt.h twiddle.h winansi.h \
 standardenc.h
dvipdfm.o: dvipdfm.c config.h system.h dvi.h error.h numbers.h \
 pdfdev.h mem.h mfileio.h pdfdoc.h pdfobj.h type1.h pdfspecial.h \
 pdfparse.h vf.h pkfont.h thumbnail.h psimage.h
epdf.o: epdf.c system.h pdfobj.h pdfdoc.h pdfspecial.h numbers.h \
 mfileio.h epdf.h mem.h
vf.o: vf.c mfileio.h numbers.h pdflimits.h system.h mem.h error.h \
 tfm.h pdfdev.h dvi.h vf.h dvicodes.h
t1crypt.o: t1crypt.c
pkfont.o: pkfont.c pkfont.h pdfobj.h mfileio.h numbers.h pdflimits.h \
 mem.h tfm.h error.h system.h
tpic.o: tpic.c tpic.h numbers.h pdfparse.h pdfobj.h mem.h mfileio.h \
 pdfdoc.h dvi.h error.h pdfdev.h
thumbnail.o: thumbnail.c config.h system.h mfileio.h numbers.h mem.h \
 pdfobj.h thumbnail.h pngimage.h
pngimage.o: pngimage.c config.h system.h mem.h pdfobj.h
ebb.o: ebb.c system.h mfileio.h numbers.h pdfobj.h jpeg.h pdfspecial.h \
 mem.h pdfparse.h config.h pngimage.h
htex.o: htex.c htex.h numbers.h pdfparse.h pdfobj.h mem.h mfileio.h \
 pdfdoc.h dvi.h error.h pdfdev.h /usr/include/ctype.h
mpost.o: mpost.c system.h mfileio.h numbers.h dvi.h error.h pdfdev.h \
 pdfobj.h pdfspecial.h pdfparse.h mpost.h mem.h pdflimits.h pdfdoc.h
psimage.o: psimage.c system.h mem.h mfileio.h numbers.h pdfobj.h \
 psimage.h pdfspecial.h epdf.h
psspecial.o: psspecial.c system.h mem.h mfileio.h numbers.h \
 psspecial.h pdfparse.h pdfobj.h pdfspecial.h psimage.h pdfdoc.h
