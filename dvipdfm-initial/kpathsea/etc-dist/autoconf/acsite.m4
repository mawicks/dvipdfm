dnl Modifications for the latest version of Autoconf for kpathsea.
dnl These changes have all been sent back to the Autoconf maintainer via
dnl bug-gnu-utils@prep.ai.mit.edu.


dnl kb_AC_LIBTOOL_REPLACE_FUNCS(FUNCTION-NAME...)
AC_DEFUN(kb_AC_LIBTOOL_REPLACE_FUNCS,
[for ac_func in $1
do
AC_CHECK_FUNC($ac_func, , [LIBTOOL_LIBOBJS="$LIBTOOL_LIBOBJS ${ac_func}.lo"])
done
AC_SUBST(LIBTOOL_LIBOBJS)dnl
])


dnl Check if gcc asm for i386 needs external symbols with an underscore.
dnl Peter Breitenlohner, April 15, 1996.
undefine([pb_AC_ASM_UNDERSCORE])
AC_DEFUN(pb_AC_ASM_UNDERSCORE,
[AC_REQUIRE_CPP()dnl
AC_CACHE_CHECK(whether gcc asm needs underscore, pb_cv_asm_underscore,
[
# Older versions of GCC asm for i386 need an underscore prepended to
# external symbols. Figure out if this is so.
pb_cv_asm_underscore=yes
AC_TRY_LINK([
extern char val ;
extern void sub () ;
#if defined (__i386__) && defined (__GNUC__) 
asm("        .align 4\n"
".globl sub\n"
"sub:\n"
"        movb \$][1,val\n"
"        ret\n");
#else
void sub () { val = 1; }
#endif /* assembler */
char val ;
], [sub], pb_cv_asm_underscore=no)])
if test "x$pb_cv_asm_underscore" = xyes; then
  AC_DEFINE(ASM_NEEDS_UNDERSCORE)
fi
])

dnl Added /lib/... for A/UX.
dnl undefine([AC_PATH_X_DIRECT])dnl

dnl Changed make to ${MAKE-make}.
dnl undefine([AC_PATH_X_XMKMF])dnl

dnl Always more junk to check.
dnl undefine([AC_PATH_XTRA])dnl

dnl Added ac_include support.
dnl undefine([AC_OUTPUT_FILES])dnl


dnl From automake distribution, by Jim Meyering:
dnl Add --enable-maintainer-mode option to configure.

AC_DEFUN(AM_MAINTAINER_MODE,
[AC_MSG_CHECKING([whether to enable maintainer-specific portions of Makefiles])
  dnl maintainer-mode is disabled by default
  AC_ARG_ENABLE(maintainer-mode,
[  --enable-maintainer-mode enable make rules and dependencies not useful
                           (and sometimes confusing) to the casual installer],
      USE_MAINTAINER_MODE=$enableval,
      USE_MAINTAINER_MODE=no)
  AC_MSG_RESULT($USE_MAINTAINER_MODE)
  if test "x$USE_MAINTAINER_MODE" = xyes; then
    MAINT=
  else
    MAINT='#M#'
  fi
  AC_SUBST(MAINT)dnl
]
)

