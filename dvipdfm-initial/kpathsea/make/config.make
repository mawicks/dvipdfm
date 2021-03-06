# config.make -- autoconf rules to remake the Makefile, c-auto.h, etc.
@MAINT@ac_dir = $(top_srcdir)/../etc/autoconf
@MAINT@autoconf = $(ac_dir)/acspecific.m4 $(ac_dir)/acgeneral.m4 $(ac_dir)/acsite.m4
@MAINT@autoheader = $(ac_dir)/acconfig.h $(ac_dir)/autoheader.m4
@MAINT@
@MAINT@# I define $(autoconf) to acgeneral.m4 and the other Autoconf files, so
@MAINT@# configure automatically gets remade in the sources with a new Autoconf
@MAINT@# release.  But it would be bad for installers with Autoconf to remake
@MAINT@# configure (not to mention require Autoconf), so I take out the variable
@MAINT@# $(autoconf) definition before release.
@MAINT@# 
@MAINT@# BTW, xt.ac isn't really required for dvipsk or dviljk, but it doesn't
@MAINT@# seem worth the trouble.
@MAINT@# 
@MAINT@configure_in = $(srcdir)/configure.in $(kpathsea_srcdir)/common.ac \
@MAINT@  $(kpathsea_srcdir)/withenable.ac $(kpathsea_srcdir)/xt.ac \
@MAINT@  $(kpathsea_srcdir_parent)/acklibtool.m4
@MAINT@$(srcdir)/configure: $(configure_in) $(autoconf)
@MAINT@	cd $(srcdir) && autoconf -m $(ac_dir)
@MAINT@
@MAINT@# And make sure that --enable-maintainer-mode is used if configure is
@MAINT@# called from the makefiles.  We do not try to do this through the
@MAINT@# cache, lest the option sneaks into a global cache file.
@MAINT@enablemaintflag = --enable-maintainer-mode

config.status: $(srcdir)/configure
	$(SHELL) $(srcdir)/configure --no-create $(enablemaintflag)

Makefile: $(srcdir)/Makefile.in config.status $(top_srcdir)/../make/*.make
	$(SHELL) config.status

# This rule isn't used for the top-level Makefile, but it doesn't hurt.
# We don't depend on config.status because configure always rewrites
# config.status, even when it doesn't change. Thus it might be newer
# than c-auto.h when we don't need to remake the latter.
c-auto.h: stamp-auto
stamp-auto: $(srcdir)/c-auto.in
	$(SHELL) config.status
	date >stamp-auto

@MAINT@# autoheader reads acconfig.h (and c-auto.h.top) automatically.
@MAINT@$(srcdir)/c-auto.in: $(srcdir)/stamp-auto.in
@MAINT@$(srcdir)/stamp-auto.in: $(configure_in) $(autoheader) \
@MAINT@  $(kpathsea_srcdir)/acconfig.h
@MAINT@	cd $(srcdir) && autoheader -m $(ac_dir) -l $(kpathsea_srcdir)
@MAINT@	date >$(srcdir)/stamp-auto.in

# End of config.make.
