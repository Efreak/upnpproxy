AC_INIT([upnpproxy], [0.1], [the_jk@yahoo.com])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([dist-bzip2 foreign color-tests parallel-tests])

AM_SILENT_RULES([yes])

AC_ISC_POSIX
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST
AC_C_INLINE
AC_C___ATTRIBUTE__

DEFINES="-D_GNU_SOURCE -D_XOPEN_SOURCE"
AX_CFLAGS_WARN_ALL(DEFINES)
AX_CFLAGS_GCC_OPTION([-Wunsafe-loop-optimizations],DEFINES)

AC_ARG_ENABLE([debug], AC_HELP_STRING([compile with debug options]),
              if test "x$enableval" = "xyes"; then
                DEFINES="$DEFINES -g -DDEBUG"
              fi,)

AC_SUBST(DEFINES)

# Types

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Headers

AC_HEADER_STDBOOL
AC_CHECK_HEADERS([stdint.h inttypes.h])

# Functions

AC_CHECK_FUNCS([strndup strnlen getline getc_unlocked ungetc_unlocked])

AC_CHECK_FUNC([getopt],
  AC_CHECK_FUNCS([getopt_long]),
  AC_MSG_ERROR([need getopt]))

# Network

AC_SEARCH_LIBS([socket], [socket],, AC_MSG_ERROR([Need socket]))
AC_SEARCH_LIBS([gethostbyname], [nsl],, AC_MSG_ERROR([Need gethostbyname]))
AC_SEARCH_LIBS([inet_ntoa], [resolv],, AC_MSG_ERROR([Need inet_ntoa]))

AC_CHECK_FUNCS([inet_ntoa inet_aton inet_ntop inet_pton inet_addr])

have_inet6=0
AC_CHECK_TYPE([struct sockaddr_in6], [have_inet6=1],, [[
#include <netinet/in.h>
]])
AC_DEFINE_UNQUOTED([HAVE_INET6], [$have_inet6], [define to 1 if struct sockaddr_in6 and the rest of IPv6 functions is available])

# uuid

AC_SEARCH_LIBS([uuid_generate], [uuid],, AC_MSG_ERROR([Need uuid_generate]))

AM_CONFIG_HEADER([config.h])
AC_OUTPUT([Makefile src/Makefile test/Makefile])
