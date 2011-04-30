# Copyright (C) 2011, Joel Klinghed

AC_INIT([upnpproxy], [0.1], [the_jk@yahoo.com])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([dist-bzip2 foreign color-tests parallel-tests])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

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
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Headers

AC_HEADER_STDBOOL
AC_CHECK_HEADERS([stdint.h inttypes.h])

# Functions

AC_CHECK_FUNCS([strndup strnlen getline getc_unlocked ungetc_unlocked])

AC_CHECK_FUNC([getopt],, AC_MSG_ERROR([need getopt]))
AC_CHECK_FUNCS([getopt_long])

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

have_uuid_generate=
have_uuid_create=

AC_CHECK_FUNC([uuid_create],have_uuid_create=1,have_uuid_create=0)

AC_SEARCH_LIBS([uuid_generate], [uuid],
               have_uuid_generate=1, have_uuid_generate=0)
if test "x$have_uuid_generate" = "x1"; then
  AC_CHECK_HEADER([uuid/uuid.h],,have_uuid_generate=0)
fi

if test "x$have_uuid_generate" != "x1" -a "x$have_uuid_create" != "x1"; then
  AC_MSG_ERROR([Need some way of creating UUID:s])
fi
AC_DEFINE_UNQUOTED([HAVE_UUID_GENERATE], [$have_uuid_generate], [define to 1 if uuid_generate is available])
AC_DEFINE_UNQUOTED([HAVE_UUID_CREATE], [$have_uuid_create], [define to 1 if uuid_create is available])

AM_CONFIG_HEADER([config.h])
AC_OUTPUT([Makefile src/Makefile test/Makefile doc/Makefile])
