#
# Local tests written by MAW
#
AC_DEFUN(AC_EXT_TIMEZONE,
[AC_MSG_CHECKING([whether time.h defines timezone as an external variable])
AC_TRY_LINK([#include <time.h>], [ timezone; ],
	[AC_MSG_RESULT(yes); AC_DEFINE(HAVE_TIMEZONE)], [AC_MSG_RESULT(no)])])
AC_DEFUN(AC_HAVE_BASENAME,
       [AC_MSG_CHECKING([whether basename is in either libgen.h or string.h])
       AC_TRY_COMPILE([#include <libgen.h>
#include <string.h>], [extern char basename(void)],
	[AC_MSG_RESULT(no)], [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_BASENAME)])])
AC_DEFUN(AC_TZ_HAS_TM_GMTOFF,
[AC_MSG_CHECKING([whether struct tz has tm_gmtoff as a member])
AC_TRY_COMPILE([#include <time.h>], [struct tm *tp; tp->tm_gmtoff],
	[AC_MSG_RESULT(yes); AC_DEFINE(HAVE_TM_GMTOFF)], [AC_MSG_RESULT(no)])])
AC_DEFUN(AC_HAS_KPSE_FORMATS,
	[AC_MSG_CHECKING([whether you have kpathsea headers and they whether they know about the required file formats])
	 AC_TRY_COMPILE([#include <stdio.h>
#include <kpathsea/tex-file.h>],
	     	        [kpse_afm_format;kpse_tex_ps_header_format;
			 kpse_type1_format;kpse_vf_format],
			[AC_MSG_RESULT(yes); AC_DEFINE(HAVE_KPSE_FORMATS)],
			[AC_MSG_RESULT(no);
AC_MSG_ERROR([AFM, PS_HEADER, and/or VF formats not found in Kpathsea header files.

This version of dvipdfm requires that kpathsea and its headers be installed.
If you are sure they are installed and in a standard place, maybe you need a
newer version of kpathsea?  You also might try setting the environment
variable CPPFLAGS or CLFAGS with -I pointing to the directory containing
the file "tex-file.h"

])])])
#
AC_DEFUN(AC_TEXMF_TREE,
  [AC_MSG_CHECKING([location of your texmf tree using kpsewhich])
TEXMF=`kpsexpand '$TEXMFMAIN' 2>/dev/null`
if test "x$TEXMF" = "x" ; then
   TEXMF='${datadir}/texmf';
fi
AC_SUBST(TEXMF)
AC_MSG_RESULT($TEXMF)])
#
# End of local tests
#
