pdfdev.o: pdfdev.c system.h mem.h error.h dvipdfm.h mfileio.h \
 numbers.h dvi.h pdfdev.h pdfobj.h tfm.h pdfdoc.h type1.h pkfont.h \
 pdfspecial.h pdfparse.h tpic.h htex.h mpost.h psspecial.h colorsp.h \
 pdflimits.h colors.h
pdfdoc.o: pdfdoc.c system.h config.h mem.h error.h dvipdfm.h mfileio.h \
 numbers.h dvi.h pdfdev.h pdfobj.h pdflimits.h pdfdoc.h pdfspecial.h \
 thumbnail.h
dvi.o: dvi.c system.h mem.h error.h dvipdfm.h mfileio.h numbers.h \
 dvi.h pdfdev.h pdfobj.h dvicodes.h pdflimits.h pdfdoc.h tfm.h vf.h
mfileio.o: mfileio.c system.h mfileio.h numbers.h error.h dvipdfm.h
mem.o: mem.c mem.h
jpeg.o: jpeg.c system.h mem.h mfileio.h numbers.h dvi.h error.h \
 dvipdfm.h pdfdev.h pdfobj.h jpeg.h pdfspecial.h
numbers.o: numbers.c system.h error.h dvipdfm.h mfileio.h numbers.h
pdfobj.o: pdfobj.c system.h config.h mem.h error.h dvipdfm.h mfileio.h \
 numbers.h pdflimits.h pdfobj.h pdfspecial.h pdfparse.h twiddle.h
pdfparse.o: pdfparse.c system.h mem.h mfileio.h numbers.h dvi.h \
 error.h dvipdfm.h pdfdev.h pdfobj.h pdfparse.h pdfspecial.h pdfdoc.h
pdfspecial.o: pdfspecial.c system.h mem.h mfileio.h numbers.h dvi.h \
 error.h dvipdfm.h pdfdev.h pdfobj.h pdflimits.h pdfspecial.h pdfdoc.h \
 pdfparse.h jpeg.h epdf.h mpost.h psimage.h config.h pngimage.h
tfm.o: tfm.c system.h mem.h error.h dvipdfm.h mfileio.h numbers.h \
 pdflimits.h
type1.o: type1.c system.h mem.h error.h dvipdfm.h mfileio.h numbers.h \
 pdfobj.h type1.h tfm.h pdfparse.h pdflimits.h t1crypt.h twiddle.h \
 winansi.h standardenc.h
dvipdfm.o: dvipdfm.c system.h config.h mem.h mfileio.h numbers.h dvi.h \
 error.h dvipdfm.h pdfdev.h pdfobj.h pdfdoc.h type1.h colorsp.h \
 pdfparse.h pdfspecial.h vf.h pkfont.h thumbnail.h psimage.h
epdf.o: epdf.c system.h mem.h mfileio.h numbers.h pdfobj.h pdfdoc.h \
 pdfspecial.h epdf.h
vf.o: vf.c system.h mfileio.h numbers.h pdflimits.h mem.h error.h \
 dvipdfm.h tfm.h pdfdev.h pdfobj.h dvi.h vf.h dvicodes.h
t1crypt.o: t1crypt.c t1crypt.h
pkfont.o: pkfont.c system.h mem.h error.h dvipdfm.h mfileio.h \
 numbers.h pkfont.h pdfobj.h pdflimits.h tfm.h
tpic.o: tpic.c system.h mem.h mfileio.h numbers.h tpic.h pdfparse.h \
 pdfobj.h pdfdoc.h dvi.h error.h dvipdfm.h pdfdev.h
thumbnail.o: thumbnail.c system.h config.h mfileio.h numbers.h mem.h \
 pdfobj.h thumbnail.h pngimage.h
pngimage.o: pngimage.c system.h config.h mem.h pdfobj.h
ebb.o: ebb.c system.h mem.h mfileio.h numbers.h pdfobj.h jpeg.h \
 pdfspecial.h pdfparse.h config.h pngimage.h
htex.o: htex.c system.h mem.h mfileio.h numbers.h dvi.h error.h \
 dvipdfm.h pdfdev.h pdfobj.h htex.h pdfparse.h pdfdoc.h
mpost.o: mpost.c system.h mem.h error.h dvipdfm.h mfileio.h numbers.h \
 dvi.h pdfdev.h pdfobj.h pdfspecial.h pdfparse.h mpost.h pdflimits.h \
 pdfdoc.h
psimage.o: psimage.c system.h config.h mem.h mfileio.h numbers.h \
 pdfobj.h psimage.h pdfspecial.h epdf.h
psspecial.o: psspecial.c system.h mem.h mfileio.h numbers.h \
 psspecial.h pdfparse.h pdfobj.h pdfspecial.h psimage.h mpost.h \
 pdfdoc.h
colorsp.o: colorsp.c system.h mem.h pdfdev.h numbers.h pdfobj.h \
 pdfparse.h pdfspecial.h dvipdfm.h
