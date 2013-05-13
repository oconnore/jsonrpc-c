AC_INIT([jsonrpc-c], [0.2], [hmng@farol.pt])
AC_CONFIG_SRCDIR([src/jsonrpc-c.c])

AM_PROG_AR
LT_INIT
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

#AC_CHECK_LIB([ev], [ev_io_init],[], [AC_MSG_ERROR([libev was not found])])
#AC_CHECK_LIB([ev],[ev_sleep],[],[AC_MSG_ERROR([libev was not found])])


AC_MSG_CHECKING([for jansson support])
AC_ARG_WITH([jansson],
    [AS_HELP_STRING([--with-jansson@<:@=PATH@:>@],[Search for jansson in PATH/include and PATH/lib])],
    [WITH_JANSSON=$withval],[WITH_JANSSON=yes])

# stollen from lighttpd's configure.ac
AC_MSG_CHECKING([for libev support])
AC_ARG_WITH([libev],
    [AS_HELP_STRING([--with-libev@<:@=PATH@:>@],[Search for libev in PATH/include and PATH/lib])],
    [WITH_LIBEV=$withval],[WITH_LIBEV=yes])

LIBJANSSON_CFLAGS=""
LIBJANSSON_LIBS=""
LIBEV_CFLAGS=""
LIBEV_LIBS=""

PKG_CHECK_MODULES([LIBJANSSON], [libjansson], [], [
    if test "$WITH_JANSSON" != "yes"; then
       LIBJANSSON_CFLAGS="-I$WITH_LIBJANSSON/include"
       LIBJANSSON_LIBS="-L$WITH_LIBJANSSON/lib -ljansson"
    else
       AC_CHECK_HEADERS([jansson.h],[
          AC_CHECK_LIB([jansson], [json_pack], [
                LIBJANSSON_LIBS="-ljansson"
             ],[
                AC_MSG_ERROR([libjansson not found])
             ]
          )],[
             AC_MSG_ERROR([libjansson not found])
          ])
    fi
])

PKG_CHECK_MODULES([LIBEV], [libev], [], [
	# no pkg-config for libev, searching manually:
	if test "$WITH_LIBEV" != "yes"; then
		LIBEV_CFLAGS="-I$WITH_LIBEV/include"
		LIBEV_LIBS="-L$WITH_LIBEV/lib -lev"
	else
		AC_CHECK_HEADERS([ev.h],[
			AC_CHECK_LIB([ev], [ev_time], [
					LIBEV_LIBS="-lev"
				],[
					AC_MSG_ERROR([libev not found])
				]
			)],[
				AC_MSG_ERROR([libev not found])
			]
		)
	fi
])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset socket strdup])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

AC_SUBST([LIBEV_CFLAGS])
AC_SUBST([LIBEV_LIBS])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 include/Makefile
 src/Makefile
 example/Makefile
])
AC_OUTPUT

