#
# Local tests written by MAW
#
AC_DEFUN(AC_EXT_TIMEZONE,
[AC_MSG_CHECKING([whether time.h defines timezone as an external variable])
AC_TRY_LINK([#include <time.h>], [ timezone; ],
	[AC_MSG_RESULT(yes); AC_DEFINE(HAVE_TIMEZONE)], [AC_MSG_RESULT(no)])])
AC_DEFUN(AC_TZ_HAS_TM_GMTOFF,
[AC_MSG_CHECKING([whether struct tz has tm_gmtoff as a member])
AC_TRY_COMPILE([#include <time.h>], [struct tm *tp; tp->tm_gmtoff],
	[AC_MSG_RESULT(yes); AC_DEFINE(HAVE_TM_GMTOFF)], [AC_MSG_RESULT(no)])])
#
# End of local tests
#




