# dist.make -- making distribution tar files.
@MAINT@top_distdir = $(distname)-$(version)
@MAINT@top_files = ChangeLog Makefile.in README configure.in configure \
@MAINT@  install.sh acklibtool.m4 config.guess config.sub klibtool \
@MAINT@  mkinstalldirs add-info-toc rename unbackslsh.awk withenable.ac
@MAINT@distdir = $(top_distdir)/$(distname)
@MAINT@kpathsea_distdir = ../$(distname)/$(top_distdir)/kpathsea
@MAINT@ln_files = AUTHORS ChangeLog INSTALL NEWS README *.in *.h *.c \
@MAINT@  configure *.make
@MAINT@
@MAINT@dist_rm_predicate = -name Makefile
@MAINT@dist: all depend pre-dist-$(distname)
@MAINT@	rm -rf $(top_distdir)*
@MAINT@	mkdir -p $(distdir)
@MAINT@	cd .. && make Makefile ./configure
@MAINT@	cd .. && ln $(top_files) $(distname)/$(top_distdir)
@MAINT@	cp -p $(top_srcdir)/../dir $(top_distdir)
@MAINT@	-ln $(ln_files) $(distdir)
@MAINT@	ln $(program_files) $(distdir)
@MAINT@	cd $(kpathsea_dir); $(MAKE) distdir=$(kpathsea_distdir) \
@MAINT@	  ln_files='$(ln_files)' distdir
@MAINT@	cp -pr ../make ../etc ../djgpp $(top_distdir)
@MAINT@	rm -rf $(top_distdir)/make/CVS
@MAINT@	rm -rf $(top_distdir)/etc/CVS
@MAINT@	rm -rf $(top_distdir)/etc/autoconf/CVS
@MAINT@	rm -rf $(top_distdir)/djgpp/CVS
@MAINT@# Remove the extra files our patterns got us.
@MAINT@	cd $(top_distdir); rm -f */c-auto.h
@MAINT@	find $(top_distdir) \( $(dist_rm_predicate) \) -exec rm '{}' \;
@MAINT@	find $(top_distdir) -name \.*texi -exec egrep -ni '	| ::|xx[^}]' \;
@MAINT@# Now handle the contrib dir.
@MAINT@	mkdir -p $(top_distdir)/contrib && \
@MAINT@	  cp -p ../contrib/{ChangeLog,INSTALL,Makefile,README,*.c,*.h} \
@MAINT@	        $(top_distdir)/contrib
@MAINT@	$(MAKE) post-dist-$(distname)
@MAINT@	cd $(distdir); test ! -r *.info || touch *.info*
@MAINT@	chmod -R a+rX,u+w $(top_distdir)
@MAINT@	GZIP=-9 tar chzf $(top_distdir).tar.gz $(top_distdir)
@MAINT@	rm -rf $(top_distdir)
# End of dist.make.
