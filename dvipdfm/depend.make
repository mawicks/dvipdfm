ascii85.o: ascii85.c 
dvi.o: dvi.c system.h c-auto.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h \
 $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h \
 $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h $(kpathsea_srcdir)/debug.h \
 $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h $(kpathsea_srcdir)/progname.h \
 $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h \
 error.h numbers.h \
 mfileio.h pdflimits.h pdfdev.h tfm.h mem.h
dvipdfm.o: dvipdfm.c \
 c-auto.h system.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h dvi.h \
 error.h mem.h pdfdoc.h \
 pdfdev.h numbers.h type1.h
ebb.o: ebb.c system.h c-auto.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h \
 $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h \
 $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h $(kpathsea_srcdir)/debug.h \
 $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h $(kpathsea_srcdir)/progname.h \
 $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h jpeg.h \
 pdfspecial.h numbers.h mem.h \
 pdfparse.h
epdf.o: epdf.c system.h c-auto.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h \
 $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h \
 $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h $(kpathsea_srcdir)/debug.h \
 $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h $(kpathsea_srcdir)/progname.h \
 $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h pdfspecial.h numbers.h mfileio.h \
 epdf.h
jpeg.o: jpeg.c \
 system.h c-auto.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h mfileio.h \
 numbers.h mem.h jpeg.h \
 pdfspecial.h dvi.h error.h
mem.o: mem.c \
 mem.h
mfileio.o: mfileio.c mfileio.h \
 numbers.h \
 error.h \
 system.h c-auto.h $(kpathsea_srcdir)/config.h $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h 
numbers.o: numbers.c numbers.h \
 error.h \
 system.h c-auto.h $(kpathsea_srcdir)/config.h $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h mfileio.h
pdfdev.o: pdfdev.c \
 pdfdev.h numbers.h \
 pdfdoc.h pdfobj.h error.h system.h c-auto.h \
 $(kpathsea_srcdir)/config.h $(kpathsea_srcdir)/c-std.h $(kpathsea_srcdir)/c-unistd.h \
 $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h type1.h mem.h \
 mfileio.h pdfspecial.h \
 pdflimits.h
pdfdoc.o: pdfdoc.c \
 c-auto.h system.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h pdflimits.h \
 error.h mem.h pdfdoc.h numbers.h
pdfobj.o: pdfobj.c \
 pdflimits.h system.h c-auto.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h \
 $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h mem.h \
 error.h mfileio.h \
 numbers.h pdfspecial.h pdfparse.h
pdfparse.o: pdfparse.c \
 system.h c-auto.h $(kpathsea_srcdir)/config.h $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h pdfparse.h \
 numbers.h pdfspecial.h pdfdoc.h pdfdev.h mem.h dvi.h error.h \
 mfileio.h
pdfspecial.o: pdfspecial.c \
 system.h \
 c-auto.h $(kpathsea_srcdir)/config.h $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h \
 $(kpathsea_srcdir)/debug.h $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h \
 $(kpathsea_srcdir)/progname.h $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h pdflimits.h \
 pdfspecial.h numbers.h pdfdoc.h pdfdev.h pdfparse.h mem.h dvi.h \
 error.h mfileio.h jpeg.h epdf.h
tfm.o: tfm.c system.h c-auto.h $(kpathsea_srcdir)/config.h \
 $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h \
 $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h \
 $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h \
 $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h $(kpathsea_srcdir)/debug.h \
 $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h $(kpathsea_srcdir)/progname.h \
 $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h pdflimits.h numbers.h error.h \
 mfileio.h mem.h 
type1.o: type1.c \
 system.h \
 c-auto.h $(kpathsea_srcdir)/config.h $(kpathsea_srcdir)/c-std.h \
 $(kpathsea_srcdir)/c-unistd.h $(kpathsea_srcdir)/systypes.h \
 $(kpathsea_srcdir)/c-memstr.h $(kpathsea_srcdir)/c-errno.h \
 $(kpathsea_srcdir)/c-minmax.h \
 $(kpathsea_srcdir)/c-limits.h $(kpathsea_srcdir)/c-proto.h $(kpathsea_srcdir)/debug.h \
 $(kpathsea_srcdir)/types.h $(kpathsea_srcdir)/lib.h $(kpathsea_srcdir)/progname.h \
 $(kpathsea_srcdir)/c-fopen.h \
 $(kpathsea_srcdir)/tex-file.h pdfobj.h mem.h \
 error.h mfileio.h numbers.h type1.h tfm.h \
 pdfparse.h pdflimits.h winansi.h
